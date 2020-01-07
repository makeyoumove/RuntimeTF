import tensorflow as tf
import convnet_builder
import datasets
import preprocessing
import os
import numpy as np
from models import model_config
from tensorflow.python import debug as tf_debug
from collections import namedtuple
from tensorflow.core.protobuf import config_pb2

config = tf.ConfigProto()
config.gpu_options.allow_growth = True

in_dir = '/home/dumbo/inception_cifar_data/convert_ilsvrc2012'
in_name = 'imagenet'
model_name = 'inception3'
batch_size = 64

tf.app.flags.DEFINE_integer("split", 1, "Split")
tf.app.flags.DEFINE_string("ps_hosts", "", "ps hosts")
tf.app.flags.DEFINE_string("worker_hosts", "", "worker hosts")
tf.app.flags.DEFINE_string("job_name", "", "'ps' / 'worker'")
tf.app.flags.DEFINE_integer("task_index", 0, "Index of task")
tf.app.flags.DEFINE_integer("batch_size", 64, "Batch size")
tf.app.flags.DEFINE_integer("iter", 100, "Iteration #")
tf.app.flags.DEFINE_string("data_name", "imagenet", "Dataset")
tf.app.flags.DEFINE_string("model_name", "inception3", "Model")
tf.app.flags.DEFINE_boolean("use_tf_layers", True, "Use_TF_Layers")
tf.app.flags.DEFINE_boolean("fp16_vars", False, "FP16_Vars")
tf.app.flags.DEFINE_boolean("use_fp16", False, "Use_FP16")
tf.app.flags.DEFINE_enum("data_format", 'NCHW', ('NHWC', 'NCHW'), "NHWCNCHW")
tf.app.flags.DEFINE_float('resnet_base_lr', None, "RESNET")
tf.app.flags.DEFINE_enum('variable_update', 'parameter_server',
                  ('parameter_server', 'replicated', 'distributed_replicated',
                   'independent', 'distributed_all_reduce',
                   'collective_all_reduce', 'horovod'),
                  'The method for managing variables: parameter_server, '
                  'replicated, distributed_replicated, independent, '
                  'distributed_all_reduce, collective_all_reduce, horovod')
tf.app.flags.DEFINE_integer('num_gpus', 1, 'the number of GPUs to run on')

params = tf.app.flags.FLAGS
in_name = params.data_name
model_name = params.model_name
batch_size = params.batch_size
niter = params.iter
ps_hosts = params.ps_hosts.split(",")
worker_hosts = params.worker_hosts.split(",")

cluster = tf.train.ClusterSpec({"ps": ps_hosts, "worker": worker_hosts})
server = tf.train.Server(cluster, job_name=params.job_name, task_index=params.task_index, config=config)

#params = namedtuple('Params', 'param')
#params.model = model_name
#params.use_datasets = True
#params.datasets_repeat_cached_sample = False
#params.datasets_num_private_threads = None
#params.datasets_use_caching = False
#params.datasets_parallel_interleave_cycle_length = None
#params.datasets_sloppy_parallel_interleave = False
#params.datasets_parallel_interleave_prefetch = None

if params.job_name == "ps":
    server.join()

elif params.job_name == "worker":
    tf.reset_default_graph()
    with tf.Graph().as_default():
        with tf.device(tf.train.replica_device_setter(worker_device="/job:worker/task:%d" % params.task_index, 
                cluster=cluster)):
            print("Prepare...")
            global_step = tf.get_variable('global_step', [], initializer=tf.constant_initializer(0), 
                    trainable=False, dtype=tf.int32)
            dataset = datasets.create_dataset(in_dir, in_name)
            model = model_config.get_model_config(model_name, in_name)(params=params)

            output_shape = model.get_input_shapes(subset='train')
            print(output_shape)

            reader = dataset.get_input_preprocessor('default')(batch_size, output_shape, 1, dtype=model.data_type, 
                    train=False, distortions=True, resize_method='bilinear')
            ds = reader.create_dataset(batch_size, 1, batch_size, dataset, 'train', 
                    False, False, None, False, None, False, None)
            it = tf.compat.v1.data.make_initializable_iterator(ds)

#           sess = tf.Session(config=config)
#           sess.run(it.initializer)

#           input_list = sess.run(it.get_next())
            input_list = it.get_next()

            shape1 = np.shape(input_list[0])
            shape2 = np.shape(input_list[1])

            shape1 = (None, shape1[1], shape1[2], shape1[3])
            shape2 = (None, shape2[1])

            print(shape1)
            print(shape2)

            X = tf.placeholder(model.data_type, shape=shape1)
            Y_Label = tf.placeholder(tf.int32, shape=shape2)

            output, extra_info = model.build_network([X, Y_Label])

            Loss = tf.reduce_mean(tf.losses.sparse_softmax_cross_entropy(labels=Y_Label, logits=output))
            train_step = tf.train.AdamOptimizer(0.005).minimize(Loss, global_step=global_step)
#            accuracy = tf.reduce_mean(tf.cast(tf.equal(tf.argmax(output, 1), tf.argmax(Y_Label, 1)), model.data_type))

            init_op = [tf.global_variables_initializer(), it.initializer]

        print("Start..")
#        total_parameters = 0
#        for variable in tf.trainable_variables():
        # shape is an array of tf.Dimension
#            shape = variable.get_shape()
#            print(shape)
#            print(len(shape))
#            variable_parameters = 1
#            for dim in shape:
#                print(dim)
#                variable_parameters *= dim.value
#            print(variable_parameters)
#            total_parameters += variable_parameters
#        print(total_parameters)
#        exit()

        sv = tf.train.Supervisor(is_chief=(params.task_index == 0), 
#                logdir="./cnn_logs", 
#                global_step=global_step, init_op=init_op)
                global_step=global_step)
            #with tf_debug.LocalCLIDebugWrapperSession(sess) as sess:
        
            #sess = tf.Session(config=config)
#    sess = sv.managed_session(server.target, config=config)
#    with sess:
        split = params.split
#        split = 4
        options = config_pb2.RunOptions(split = split)

        with sv.managed_session(server.target, config=config) as sess:
            sess.graph._unsafe_unfinalize()
            sess.run([tf.global_variables_initializer(), it.initializer, tf.local_variables_initializer()])
#            split_step = tf.assign_sub(global_step, split - 1)
            step = 0
            next_item = it.get_next()
            while not sv.should_stop() and step < niter:
#        for i in range(1000):
                images, labels = sess.run(next_item)
#                res, step = sess.run([train_step, global_step], feed_dict = {X:images, Y_Label:labels})
                res = sess.run(train_step, feed_dict = {X:images, Y_Label:labels}, options=options)
#                sess.run(split_step)
#                step = sess.run(global_step)
                step = step + 1
#                print("STEP", step)
                if step % 50 == 0 :
                    images, labels = sess.run(it.get_next())
                    print("Worker ", params.task_index, " Step ", step, ":", sess.run(Loss, feed_dict = {X:images, Y_Label:labels}, options=options))

        sv.stop()
        print("END")
