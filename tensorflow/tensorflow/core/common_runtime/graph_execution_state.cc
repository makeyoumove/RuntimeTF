/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/common_runtime/graph_execution_state.h"

#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/metrics.h"
#include "tensorflow/core/common_runtime/optimization_registry.h"
#include "tensorflow/core/common_runtime/placer.h"
#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/graph.pb_text.h"
#include "tensorflow/core/framework/graph_def_util.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/framework/versions.pb.h"
#include "tensorflow/core/graph/algorithm.h"
#include "tensorflow/core/graph/collective_order.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/graph_constructor.h"
#include "tensorflow/core/graph/subgraph.h"
#include "tensorflow/core/graph/tensor_id.h"
#include "tensorflow/core/graph/validate.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/gtl/flatset.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/util/device_name_utils.h"
#include "tensorflow/core/util/util.h"

#ifndef IS_MOBILE_PLATFORM
#include "tensorflow/core/grappler/clusters/virtual_cluster.h"
#include "tensorflow/core/grappler/grappler_item.h"
#include "tensorflow/core/grappler/optimizers/meta_optimizer.h"
#endif  // IS_MOBILE_PLATFORM

namespace tensorflow {

GraphExecutionState::GraphExecutionState(
    GraphDef* graph_def, const GraphExecutionStateOptions& options)
    : stateful_placements_(options.stateful_placements),
      device_set_(options.device_set),
      session_options_(options.session_options),
      session_handle_(options.session_handle),
      flib_def_(new FunctionLibraryDefinition(OpRegistry::Global(),
                                              graph_def->library())),
      graph_(nullptr) {
  // NOTE(mrry): GraphDef does not have a move constructor, so we pass
  // a non-const pointer and use `Swap()` to transfer the contents
  // without copying.
  original_graph_def_.Swap(graph_def);
  // TODO(mrry): Publish placement visualizations or handle the log
  // placement option.
}

GraphExecutionState::~GraphExecutionState() {
  node_name_to_cost_id_map_.clear();
  delete graph_;
}

/* static */ Status GraphExecutionState::MakeForBaseGraph(
    GraphDef* graph_def, const GraphExecutionStateOptions& options,
    std::unique_ptr<GraphExecutionState>* out_state) {
#ifndef __ANDROID__
  VLOG(4) << "Graph proto is \n" << graph_def->DebugString();
#endif  // __ANDROID__

  std::unique_ptr<GraphExecutionState> ret(
      new GraphExecutionState(graph_def, options));

  TF_RETURN_IF_ERROR(
      AddDefaultAttrsToGraphDef(&ret->original_graph_def_, *ret->flib_def_, 0));
  // TODO(mrry): Refactor InitBaseGraph() so that we don't have to
  // pass an empty BuildGraphOptions (that isn't going to be used when
  // place_pruned_graph is false).
  if (!ret->session_options_->config.graph_options().place_pruned_graph()) {
    TF_RETURN_IF_ERROR(ret->InitBaseGraph(BuildGraphOptions()));
  }
  *out_state = std::move(ret);
  return Status::OK();
}

/* static */ Status GraphExecutionState::MakeForPrunedGraph(
    const FunctionDefLibrary& func_def_lib,
    const GraphExecutionStateOptions& options, const GraphDef& graph_def,
    const BuildGraphOptions& subgraph_options,
    std::unique_ptr<GraphExecutionState>* out_state,
    std::unique_ptr<ClientGraph>* out_client_graph) {
  DCHECK(options.session_options->config.graph_options().place_pruned_graph());
  // NOTE(mrry): This makes a copy of `graph_def`, which is
  // regrettable. We could make `GraphDef` objects sharable between
  // execution states to optimize pruned graph execution, but since
  // this case is primarily used for interactive sessions, we make the
  // bet that graph construction is not performance-critical. (Note
  // also that the previous version used `Extend()`, which is strictly
  // more expensive than copying a `GraphDef`.)
  GraphDef temp(graph_def);
  std::unique_ptr<GraphExecutionState> ret(
      new GraphExecutionState(&temp, options));
  TF_RETURN_IF_ERROR(
      AddDefaultAttrsToGraphDef(&ret->original_graph_def_, *ret->flib_def_, 0));
  TF_RETURN_IF_ERROR(ret->InitBaseGraph(subgraph_options));
  TF_RETURN_IF_ERROR(ret->BuildGraph(subgraph_options, out_client_graph));
  *out_state = std::move(ret);
  return Status::OK();
}

Status GraphExecutionState::Extend(
    const GraphDef& extension_def,
    std::unique_ptr<GraphExecutionState>* out) const {
  GraphDef gdef;

  // 1. Copy the function library.
  TF_RETURN_IF_ERROR(flib_def_->AddLibrary(extension_def.library()));
  *gdef.mutable_library() = flib_def_->ToProto();

  // 2. Build an index of the new node names.
  std::unordered_set<string> new_names;
  for (const NodeDef& node : extension_def.node()) {
    new_names.insert(node.name());
  }

  // 3. Add the non-duplicates from the old graph to the new graph.
  //    Return an error if the same node name appears in both the
  //    old graph and the extension.
  for (const NodeDef& node : original_graph_def_.node()) {
    if (new_names.count(node.name()) == 0) {
      *gdef.add_node() = node;
    } else {
      return errors::InvalidArgument(tensorflow::strings::Printf(
          "GraphDef argument to Extend includes node '%s', which was created "
          "by a previous call to Create or Extend in this session.",
          node.name().c_str()));
    }
  }

  // 4. Merge the versions field.
  int old_node_size = gdef.node_size();
  gdef.mutable_node()->MergeFrom(extension_def.node());
  TF_RETURN_IF_ERROR(
      AddDefaultAttrsToGraphDef(&gdef, *flib_def_, old_node_size));
  // Merge versions
  if (gdef.has_versions()) {
    if (gdef.versions().producer() != extension_def.versions().producer()) {
      return errors::InvalidArgument(
          "Can't extend GraphDef at version ", gdef.versions().producer(),
          " with graph at version ", extension_def.versions().producer());
    }
    VersionDef* versions = gdef.mutable_versions();
    versions->set_min_consumer(std::max(
        versions->min_consumer(), extension_def.versions().min_consumer()));
    if (extension_def.versions().bad_consumers_size()) {
      // Add new bad_consumers that aren't already marked bad.
      //
      // Note: This implementation is quadratic time if there are many calls to
      // ExtendLocked with many bad consumers.  Since this is unlikely, and
      // fixing it would require data structures outside of this routine,
      // quadratic time it is.
      auto* bad_consumers = versions->mutable_bad_consumers();
      const std::unordered_set<int> existing(bad_consumers->begin(),
                                             bad_consumers->end());
      for (const int v : extension_def.versions().bad_consumers()) {
        if (existing.find(v) == existing.end()) {
          bad_consumers->Add(v);
        }
      }
    }

  } else {
    gdef.mutable_versions()->CopyFrom(extension_def.versions());
  }

  // 5. Validate that the final graphdef is valid.
  if (gdef.versions().producer() >= 5) {
    // Validate the graph: we assume that merging two valid graphs
    // should maintain graph validity.
    TF_RETURN_IF_ERROR(graph::ValidateGraphDef(gdef, *flib_def_));
  }

  // 6. Add the extension.
  GraphExecutionStateOptions combined_options;
  combined_options.device_set = device_set_;
  combined_options.session_options = session_options_;
  combined_options.session_handle = session_handle_;
  combined_options.stateful_placements = stateful_placements_;

  // NOTE(mrry): `gdef` is no longer valid after the constructor
  // executes.
  std::unique_ptr<GraphExecutionState> new_execution_state(
      new GraphExecutionState(&gdef, combined_options));

  TF_RETURN_IF_ERROR(AddDefaultAttrsToGraphDef(
      &new_execution_state->original_graph_def_, *flib_def_, 0));
  if (!session_options_->config.graph_options().place_pruned_graph()) {
    // TODO(mrry): Refactor InitBaseGraph() so that we don't have to
    // pass an empty BuildGraphOptions (that isn't going to be used
    // when place_pruned_graph is false).
    TF_RETURN_IF_ERROR(new_execution_state->InitBaseGraph(BuildGraphOptions()));
  }
  *out = std::move(new_execution_state);

  // TODO(mrry): This is likely to be used for non-throughput-sensitive
  // interactive workloads, but in future we may want to transfer other
  // parts of the placement and/or cost model.
  return Status::OK();
}

void GraphExecutionState::SaveStatefulNodes(Graph* graph) {
  for (Node* n : graph->nodes()) {
    if (n->op_def().is_stateful()) {
      VLOG(2) << "Saving " << n->DebugString();
      stateful_placements_[n->name()] = n->assigned_device_name();
    }
  }
}

void GraphExecutionState::RestoreStatefulNodes(Graph* graph) {
  for (Node* n : graph->nodes()) {
    if (n->op_def().is_stateful()) {
      auto iter = stateful_placements_.find(n->name());
      if (iter != stateful_placements_.end()) {
        n->set_assigned_device_name(iter->second);
        VLOG(2) << "Restored " << n->DebugString();
      }
    }
  }
}

namespace {

class TensorConnectionPruneRewrite : public subgraph::PruneRewrite {
 public:
  TensorConnectionPruneRewrite(const string* endpoint_name,
                               NodeBuilder::NodeOut from_tensor)
      : subgraph::PruneRewrite(endpoint_name, nullptr /* device_info */),
        from_tensor_(std::move(from_tensor)) {}

  Status AddNode(Graph* g, NodeBuilder::NodeOut feed_tensor,
                 Node** out_node) override {
    Status s;
    auto check_no_cycle_fn = [this, feed_tensor, &s](Node* n) {
      if (n == feed_tensor.node) {
        s.Update(errors::InvalidArgument(
            "Requested Tensor connection between nodes \"",
            feed_tensor.node->name(), "\" and \"", from_tensor_.node->name(),
            "\" would create a cycle."));
      }
    };
    ReverseDFSFrom(*g, {from_tensor_.node}, std::move(check_no_cycle_fn),
                   nullptr);
    TF_RETURN_IF_ERROR(s);

    TF_RETURN_IF_ERROR(
        NodeBuilder(strings::StrCat("_identity_", feed_tensor.node->name(), "_",
                                    feed_tensor.index),
                    "Identity")
            .Input(from_tensor_)
            .Attr("T",
                  BaseType(from_tensor_.node->output_type(from_tensor_.index)))
            .Finalize(g, out_node));

    (*out_node)->set_assigned_device_name(
        feed_tensor.node->assigned_device_name());
    return Status::OK();
  }

 private:
  NodeBuilder::NodeOut from_tensor_;
};

template <class Map>
Status LookupDevice(const DeviceSet& device_set, const string& tensor_name,
                    const Map& tensor2device,
                    const tensorflow::DeviceAttributes** out_device_attrs) {
  *out_device_attrs = nullptr;
  if (tensor2device.empty()) {
    *out_device_attrs = &device_set.client_device()->attributes();
    return Status::OK();
  }
  const auto it = tensor2device.find(tensor_name);
  if (it == tensor2device.end()) {
    *out_device_attrs = &device_set.client_device()->attributes();
    return Status::OK();
  }
  DeviceNameUtils::ParsedName parsed_name;
  if (!DeviceNameUtils::ParseFullName(it->second, &parsed_name)) {
    return errors::InvalidArgument("Invalid device name ('", it->second,
                                   "') provided for the tensor '", tensor_name,
                                   "' in CallableOptions");
  }
  Device* device = device_set.FindDeviceByName(
      DeviceNameUtils::ParsedNameToString(parsed_name));
  if (device == nullptr) {
    return errors::InvalidArgument("Device '", it->second,
                                   "' specified for tensor '", tensor_name,
                                   "' in CallableOptions does not exist");
  }
  *out_device_attrs = &device->attributes();
  return Status::OK();
}

struct TensorAndDevice {
  // WARNING: backing memory for the 'tensor' field is NOT owend.
  const TensorId tensor;
  // WARNING: device pointer is not owned, so must outlive TensorAndDevice.
  const DeviceAttributes* device;
};

// Tensors of some DataTypes cannot placed in device memory as feeds or
// fetches. Validate against a whitelist of those known to work.
bool IsFeedAndFetchSupported(DataType dtype, const string& device_type) {
  // The mechanism for supporting feeds of device-backed Tensors requires
  // the _Arg kernel to be registered for the corresponding type (and that
  // the input to the kernel be in device and not host memory).
  //
  // The mechanism for supporting fetches of device-backed Tensors requires
  // the _Retval kernel to be registered for the corresponding type (and
  // that the output is produced in device and not host memory).
  //
  // For now, we return true iff there are _Arg AND _Retval kernels for dtype on
  // the device. False negatives are okay, false positives would be bad.
  //
  // TODO(ashankar): Instead of a whitelist here, perhaps we could query
  // the kernel registry for _Arg and _Retval kernels instead.
  if (device_type == DEVICE_CPU) return true;
  if (device_type != DEVICE_GPU) return false;
  switch (dtype) {
    case DT_BFLOAT16:
    case DT_BOOL:
    case DT_COMPLEX128:
    case DT_COMPLEX64:
    case DT_DOUBLE:
    case DT_FLOAT:
    case DT_HALF:
    case DT_INT16:
    case DT_INT64:
    case DT_INT8:
    case DT_UINT16:
    case DT_UINT8:
      return true;
    default:
      return false;
  }
}

Status ValidateFeedAndFetchDevices(
    const Graph& graph,
    const std::vector<TensorAndDevice>& tensors_and_devices) {
  if (tensors_and_devices.empty()) return Status::OK();
  std::vector<bool> found(tensors_and_devices.size(), false);
  for (const Node* node : graph.nodes()) {
    // Linearly looping through all nodes and then all feed+fetch tensors isn't
    // quite efficient. At the time of this writing, the expectation was that
    // tensors_and_devices.size() is really small in practice, so this won't be
    // problematic.
    // Revist and make a more efficient lookup possible if needed (e.g., perhaps
    // Graph can maintain a map from node name to Node*).
    for (int i = 0; i < tensors_and_devices.size(); ++i) {
      const TensorAndDevice& td = tensors_and_devices[i];
      if (td.tensor.first != node->name()) continue;
      found[i] = true;
      TF_RETURN_IF_ERROR(graph.IsValidOutputTensor(node, td.tensor.second));
      const DataType dtype = node->output_type(td.tensor.second);
      if (!IsFeedAndFetchSupported(dtype, td.device->device_type())) {
        return errors::Unimplemented(
            "Cannot feed or fetch tensor '", td.tensor.ToString(),
            "' from device ", td.device->name(), " as feeding/fetching from ",
            td.device->device_type(), " devices is not yet supported for ",
            DataTypeString(dtype), " tensors");
      }
    }
  }
  for (int i = 0; i < found.size(); ++i) {
    if (!found[i]) {
      return errors::InvalidArgument(
          "Tensor ", tensors_and_devices[i].tensor.ToString(),
          ", specified in either feed_devices or fetch_devices was not found "
          "in the Graph");
    }
  }
  return Status::OK();
}

Status GetFeedShapeAndTypeFromAttribute(const NodeDef& node,
                                        PartialTensorShape* shape,
                                        DataType* type) {
  static const gtl::FlatSet<string>* const kHasExplicitShapeAttribute =
      CHECK_NOTNULL((new gtl::FlatSet<string>{
          "Placeholder", "PlaceholderV2", "PlaceholderWithDefault",
          "ParallelConcat", "ImmutableConst", "_ParallelConcatStart",
          "InfeedDequeue", "OutfeedDequeue", "CollectiveBcastSend",
          "CollectiveBcastRecv", "AccumulateNV2", "VariableV2", "Variable",
          "TemporaryVariable", "NcclBroadcast", "_ScopedAllocator",
          "_ScopedAllocatorConcat"}));

  // All the node types handled here have their output datatype set in
  // either attribute 'dtype' or 'T'.
  if (!GetNodeAttr(node, "dtype", type).ok() &&
      !GetNodeAttr(node, "T", type).ok()) {
    return errors::InvalidArgument(
        "Could not determine output type for feed node: ", node.name(),
        " of type ", node.op());
  }

  // First handle the case of feeding a const node.
  if (node.op() == "Const" && HasNodeAttr(node, "value")) {
    *shape =
        PartialTensorShape(node.attr().at("value").tensor().tensor_shape());
  } else if (kHasExplicitShapeAttribute->find(node.op()) !=
             kHasExplicitShapeAttribute->end()) {
    TF_RETURN_IF_ERROR(GetNodeAttr(node, "shape", shape));
  } else {
    return errors::InvalidArgument("Could not determine shape for feed node: ",
                                   node.name(), " of type ", node.op());
  }
  return Status::OK();
}

}  // namespace

Status GraphExecutionState::PruneGraph(
    const BuildGraphOptions& options, Graph* graph,
    subgraph::RewriteGraphMetadata* out_rewrite_metadata) {
  std::vector<std::unique_ptr<subgraph::PruneRewrite>> feed_rewrites;
  feed_rewrites.reserve(options.callable_options.feed_size());
  std::vector<std::unique_ptr<subgraph::PruneRewrite>> fetch_rewrites;
  fetch_rewrites.reserve(options.callable_options.fetch_size());
  if (options.use_function_convention) {
    std::vector<TensorAndDevice> tensors_and_devices;
    for (int i = 0; i < options.callable_options.feed_size(); ++i) {
      // WARNING: feed MUST be a reference, since ArgFeedRewrite and
      // tensors_and_devices holds on to its address.
      const string& feed = options.callable_options.feed(i);
      const DeviceAttributes* device_info;
      TF_RETURN_IF_ERROR(LookupDevice(*device_set_, feed,
                                      options.callable_options.feed_devices(),
                                      &device_info));
      feed_rewrites.emplace_back(
          new subgraph::ArgFeedRewrite(&feed, device_info, i));
      tensors_and_devices.push_back({ParseTensorName(feed), device_info});
    }
    if (!options.callable_options.fetch_devices().empty() &&
        !options.callable_options.fetch_skip_sync()) {
      return errors::Unimplemented(
          "CallableOptions.fetch_skip_sync = false is not yet implemented. You "
          "can set it to true instead, but MUST ensure that Device::Sync() is "
          "invoked on the Device corresponding to the fetched tensor before "
          "dereferencing the Tensor's memory.");
    }
    for (int i = 0; i < options.callable_options.fetch_size(); ++i) {
      // WARNING: fetch MUST be a reference, since RetvalFetchRewrite and
      // tensors_and_devices holds on to its address.
      const string& fetch = options.callable_options.fetch(i);
      const DeviceAttributes* device_info;
      TF_RETURN_IF_ERROR(LookupDevice(*device_set_, fetch,
                                      options.callable_options.fetch_devices(),
                                      &device_info));
      fetch_rewrites.emplace_back(
          new subgraph::RetvalFetchRewrite(&fetch, device_info, i));
      tensors_and_devices.push_back({ParseTensorName(fetch), device_info});
    }
    TF_RETURN_IF_ERROR(
        ValidateFeedAndFetchDevices(*graph, tensors_and_devices));
  } else {
    if (!options.callable_options.feed_devices().empty() ||
        !options.callable_options.fetch_devices().empty()) {
      return errors::Unimplemented(
          "CallableOptions::feed_devices and CallableOptions::fetch_devices "
          "to configure feeding/fetching tensors to/from device memory is not "
          "yet supported when using a remote session.");
    }
    const DeviceAttributes* device_info =
        &device_set_->client_device()->attributes();
    for (const string& feed : options.callable_options.feed()) {
      feed_rewrites.emplace_back(
          new subgraph::RecvFeedRewrite(&feed, device_info));
    }
    for (const string& fetch : options.callable_options.fetch()) {
      fetch_rewrites.emplace_back(
          new subgraph::SendFetchRewrite(&fetch, device_info));
    }
  }

  for (const TensorConnection& tensor_connection :
       options.callable_options.tensor_connection()) {
    Node* from_node = nullptr;
    TensorId from_id(ParseTensorName(tensor_connection.from_tensor()));

    for (Node* n : graph->nodes()) {
      if (n->name() == from_id.first) {
        from_node = n;
        break;
      }
    }
    if (from_node == nullptr) {
      return errors::InvalidArgument(
          "Requested tensor connection from unknown node: \"",
          tensor_connection.to_tensor(), "\".");
    }
    if (from_id.second >= from_node->num_outputs()) {
      return errors::InvalidArgument(
          "Requested tensor connection from unknown edge: \"",
          tensor_connection.to_tensor(),
          "\" (actual number of outputs = ", from_node->num_outputs(), ").");
    }

    feed_rewrites.emplace_back(new TensorConnectionPruneRewrite(
        &tensor_connection.to_tensor(), {from_node, from_id.second}));
  }

  std::vector<string> target_node_names(
      options.callable_options.target().begin(),
      options.callable_options.target().end());
  TF_RETURN_IF_ERROR(subgraph::RewriteGraphForExecution(
      graph, feed_rewrites, fetch_rewrites, target_node_names,
      out_rewrite_metadata));

  CHECK_EQ(out_rewrite_metadata->feed_types.size(),
           options.callable_options.feed_size() +
               options.callable_options.tensor_connection_size());
  for (int i = 0; i < options.callable_options.tensor_connection_size(); ++i) {
    out_rewrite_metadata->feed_types.pop_back();
  }
  return Status::OK();
}

Status GraphExecutionState::InitBaseGraph(const BuildGraphOptions& options) {
  const GraphDef* graph_def = &original_graph_def_;

  std::unique_ptr<Graph> new_graph(new Graph(OpRegistry::Global()));
  GraphConstructorOptions opts;
  TF_RETURN_IF_ERROR(ConvertGraphDefToGraph(opts, *graph_def, new_graph.get()));
  if (session_options_ &&
      session_options_->config.graph_options().place_pruned_graph()) {
    // Rewrite the graph before placement.
    rewrite_metadata_.reset(new subgraph::RewriteGraphMetadata);
    TF_RETURN_IF_ERROR(
        PruneGraph(options, new_graph.get(), rewrite_metadata_.get()));
  }

  // Save stateful placements before placing.
  RestoreStatefulNodes(new_graph.get());

  GraphOptimizationPassOptions optimization_options;
  optimization_options.session_handle = session_handle_;
  optimization_options.session_options = session_options_;
  optimization_options.graph = &new_graph;
  optimization_options.flib_def = flib_def_.get();
  optimization_options.device_set = device_set_;

  TF_RETURN_IF_ERROR(OptimizationPassRegistry::Global()->RunGrouping(
      OptimizationPassRegistry::PRE_PLACEMENT, optimization_options));

  Placer placer(new_graph.get(), device_set_, /* default_device= */ nullptr,
                session_options_ == nullptr ||
                    session_options_->config.allow_soft_placement(),
                session_options_ != nullptr &&
                    session_options_->config.log_device_placement());
  // TODO(mrry): Consider making the Placer cancelable.
  TF_RETURN_IF_ERROR(placer.Run());

  TF_RETURN_IF_ERROR(OptimizationPassRegistry::Global()->RunGrouping(
      OptimizationPassRegistry::POST_PLACEMENT, optimization_options));

  for (const Node* n : new_graph->nodes()) {
    VLOG(2) << "Mapping " << n->name() << " to " << n->cost_id();
    node_name_to_cost_id_map_[n->name()] = n->cost_id();
  }

  SaveStatefulNodes(new_graph.get());
  graph_ = new_graph.release();
  return Status::OK();
}

Status GraphExecutionState::OptimizeGraph(
    const BuildGraphOptions& options, std::unique_ptr<Graph>* optimized_graph,
    std::unique_ptr<FunctionLibraryDefinition>* optimized_flib) {
#ifndef IS_MOBILE_PLATFORM
  if (session_options_->config.graph_options().place_pruned_graph()) {
    return errors::InvalidArgument("Can't optimize a pruned graph");
  }

  if (grappler::MetaOptimizerEnabled(session_options_->config)) {
    grappler::GrapplerItem item;
    item.id = "tf_graph";
    graph_->ToGraphDef(&item.graph);

    // It's ok to skip invalid device annotations in Grappler.
    Status inferred_devices = item.InferDevicesFromGraph();
    if (!inferred_devices.ok()) {
      VLOG(3) << inferred_devices.error_message();
    }

    // TODO(b/114748242): Add a unit test to test this bug fix.
    if (flib_def_) {
      *item.graph.mutable_library() = flib_def_->ToProto();
    }

    item.fetch.insert(item.fetch.end(),
                      options.callable_options.fetch().begin(),
                      options.callable_options.fetch().end());
    item.fetch.insert(item.fetch.end(),
                      options.callable_options.target().begin(),
                      options.callable_options.target().end());

    for (const TensorConnection& tensor_connection :
         options.callable_options.tensor_connection()) {
      item.fetch.push_back(tensor_connection.from_tensor());
    }

    if (!(options.callable_options.feed().empty() &&
          options.callable_options.tensor_connection().empty())) {
      std::unordered_set<string> feeds;
      for (const string& feed : options.callable_options.feed()) {
        TensorId id = ParseTensorName(feed);
        if (id.second != 0) {
          return errors::InvalidArgument("Unsupported feed: ", feed);
        }
        feeds.emplace(id.first);
      }
      for (const TensorConnection& tensor_connection :
           options.callable_options.tensor_connection()) {
        TensorId id = ParseTensorName(tensor_connection.to_tensor());
        if (id.second != 0) {
          return errors::InvalidArgument("Unsupported feed: ",
                                         tensor_connection.to_tensor());
        }
        feeds.emplace(id.first);
      }
      for (const NodeDef& node : original_graph_def_.node()) {
        if (feeds.find(node.name()) == feeds.end()) {
          continue;
        }
        // Get the type and shape of the feed node.
        PartialTensorShape partial_shape;
        DataType type;
        TF_RETURN_IF_ERROR(
            GetFeedShapeAndTypeFromAttribute(node, &partial_shape, &type));
        // If the shape of the placeholder is only partially known, we are free
        // to set unknown dimensions of its shape to any value we desire. We
        // choose 0 to minimize the memory impact. Note that this only matters
        // if an optimizer chooses to run the graph.
        TensorShape shape;
        if (partial_shape.unknown_rank()) {
          shape = TensorShape({0});
        } else {
          for (int i = 0; i < partial_shape.dims(); ++i) {
            if (partial_shape.dim_size(i) < 0) {
              partial_shape.set_dim(i, 0);
            }
          }
          if (!partial_shape.AsTensorShape(&shape)) {
            return errors::InvalidArgument(
                "Could not derive shape for feed node: ", node.DebugString());
          }
        }

        Tensor fake_input(type, shape);
        item.feed.emplace_back(node.name(), fake_input);
      }
    }

    Device* cpu_device = nullptr;
    for (const auto& device : device_set_->devices()) {
      if (device->parsed_name().id == 0 &&
          StringPiece(device->parsed_name().type) == "CPU" &&
          device->GetAllocator(AllocatorAttributes()) != nullptr) {
        cpu_device = device;
      }
    }
    grappler::VirtualCluster cluster(device_set_);
    GraphDef new_graph;
    TF_RETURN_IF_ERROR(grappler::RunMetaOptimizer(
        item, session_options_->config, cpu_device, &cluster, &new_graph));

    // Merge optimized graph function library with an original library.
    // Optimized graph might have new functions specialized for it's
    // instantiation context (see Grappler function optimizer), and modified
    // function body for the existing functions.
    optimized_flib->reset(new FunctionLibraryDefinition(*flib_def_));

    for (const FunctionDef& fdef : new_graph.library().function()) {
      const string& func_name = fdef.signature().name();

      if ((*optimized_flib)->Contains(func_name)) {
        VLOG(3) << "Replace function: name=" << func_name;
        TF_RETURN_IF_ERROR((*optimized_flib)->ReplaceFunction(func_name, fdef));
      } else {
        VLOG(3) << "Add new function: name=" << func_name;
        TF_RETURN_IF_ERROR((*optimized_flib)->AddFunctionDef(fdef));
      }
    }

    optimized_graph->reset(new Graph(OpRegistry::Global()));

    GraphConstructorOptions opts;
    opts.allow_internal_ops = true;
    TF_RETURN_IF_ERROR(
        ConvertGraphDefToGraph(opts, new_graph, optimized_graph->get()));
    // The graph conversion sets the requested device names but not the
    // assigned device names. However, since at this point the graph is placed
    // TF expects an assigned device name for every node. Therefore we copy
    // the requested device into the assigned device field.
    for (Node* node : optimized_graph->get()->nodes()) {
      node->set_assigned_device_name(node->requested_device());
    }
    return Status::OK();
  } else {
    return errors::InvalidArgument("Meta Optimizer disabled");
  }
#else
  return errors::InvalidArgument("Mobile platforms not supported");
#endif  // IS_MOBILE_PLATFORM
}

Status GraphExecutionState::BuildGraph(const BuildGraphOptions& options,
                                       std::unique_ptr<ClientGraph>* out) {
  VLOG(1) << "BuildGraph";
  const uint64 start_time_usecs = Env::Default()->NowMicros();
  if (!graph_) {
    // It is only valid to call this method directly when the original graph
    // was created with the option `place_pruned_graph == false`.
    return errors::Internal(
        "Attempted to prune a graph that has not been fully initialized.");
  }

  // Grappler optimization might change the structure of a graph itself, and
  // also it can add/prune functions to/from the library.
  std::unique_ptr<Graph> optimized_graph;
  std::unique_ptr<FunctionLibraryDefinition> optimized_flib;

  Status s = OptimizeGraph(options, &optimized_graph, &optimized_flib);
  if (!s.ok()) {
    VLOG(2) << "Grappler optimization failed. Error: " << s.error_message();
    // Simply copy the original graph and the function library if we couldn't
    // optimize it.
    optimized_graph.reset(new Graph(flib_def_.get()));
    CopyGraph(*graph_, optimized_graph.get());
    optimized_flib.reset(new FunctionLibraryDefinition(*flib_def_));
  }

//	std::cout << "KST_CHECK_FLIB " << optimized_flib.get()->num_functions() << " " << flib_def_.get()->num_functions() << "\n";

  subgraph::RewriteGraphMetadata rewrite_metadata;
  if (session_options_ == nullptr ||
      !session_options_->config.graph_options().place_pruned_graph()) {
    TF_RETURN_IF_ERROR(
        PruneGraph(options, optimized_graph.get(), &rewrite_metadata));
  } else {
    // This GraphExecutionState represents a graph that was
    // pruned when this was constructed, so we copy the metadata from
    // a member variable.
    CHECK(rewrite_metadata_);
    rewrite_metadata = *rewrite_metadata_;
  }

  CHECK_EQ(options.callable_options.feed_size(),
           rewrite_metadata.feed_types.size());
  CHECK_EQ(options.callable_options.fetch_size(),
           rewrite_metadata.fetch_types.size());

  // TODO(andydavis): Clarify optimization pass requirements around CostModel.
  GraphOptimizationPassOptions optimization_options;
  optimization_options.session_options = session_options_;
  optimization_options.graph = &optimized_graph;
  optimization_options.flib_def = optimized_flib.get();
  optimization_options.device_set = device_set_;

  TF_RETURN_IF_ERROR(OptimizationPassRegistry::Global()->RunGrouping(
      OptimizationPassRegistry::POST_REWRITE_FOR_EXEC, optimization_options));

  int64 collective_graph_key = options.collective_graph_key;
  if (collective_graph_key == BuildGraphOptions::kNoCollectiveGraphKey) {
    // BuildGraphOptions does not specify a collective_graph_key.  Check all
    // nodes in the Graph and FunctionLibraryDefinition for collective ops and
    // if found, initialize a collective_graph_key as a hash of the ordered set
    // of instance keys.
    std::set<int32> instance_key_set;
    for (Node* node : optimized_graph->nodes()) {
      if (node->IsCollective()) {
        int32 instance_key;
        TF_RETURN_IF_ERROR(
            GetNodeAttr(node->attrs(), "instance_key", &instance_key));
        instance_key_set.emplace(instance_key);
      } else {
        const FunctionDef* fdef = optimized_flib->Find(node->def().op());
        if (fdef != nullptr) {
          for (const NodeDef& ndef : fdef->node_def()) {
            if (ndef.op() == "CollectiveReduce" ||
                ndef.op() == "CollectiveBcastSend" ||
                ndef.op() == "CollectiveBcastRecv") {
              int32 instance_key;
              TF_RETURN_IF_ERROR(
                  GetNodeAttr(ndef, "instance_key", &instance_key));
              instance_key_set.emplace(instance_key);
            }
          }
        }
      }
    }
    if (!instance_key_set.empty()) {
      uint64 hash = 0x8774aa605c729c72ULL;
      for (int32 instance_key : instance_key_set) {
        hash = Hash64Combine(instance_key, hash);
      }
      collective_graph_key = hash;
    }
  }

  // Make collective execution order deterministic if needed.
  if (options.collective_order != GraphCollectiveOrder::kNone) {
    TF_RETURN_IF_ERROR(
        OrderCollectives(optimized_graph.get(), options.collective_order));
  }

  // Copy the extracted graph in order to make its node ids dense,
  // since the local CostModel used to record its stats is sized by
  // the largest node id.
  std::unique_ptr<ClientGraph> dense_copy(
      new ClientGraph(std::move(optimized_flib), rewrite_metadata.feed_types,
                      rewrite_metadata.fetch_types, collective_graph_key));
  CopyGraph(*optimized_graph, &dense_copy->graph);

  // TODO(vrv): We should check invariants of the graph here.
  metrics::UpdateGraphBuildTime(Env::Default()->NowMicros() - start_time_usecs);
  *out = std::move(dense_copy);
  return Status::OK();
}

}  // namespace tensorflow
