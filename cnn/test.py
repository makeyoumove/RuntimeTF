import tensorflow as tf
import convnet_builder
import datasets
import preprocessing
import os
import numpy as np
from models import model_config
from tensorflow.python import debug as tf_debug
from collections import namedtuple

config = tf.ConfigProto()
config.gpu_options.allow_growth = True

sess = tf.Session(config=config)

in_dir = '/home/ssl/inception_cifar_data/convert_ilsvrc2012'
in_name = 'imagenet'
model_name = 'inception3'
batch_size = 64

params = namedtuple('Params', 'param')
params.model = model_name
params.use_datasets = True
params.datasets_repeat_cached_sample = False
params.datasets_num_private_threads = None
params.datasets_use_caching = False
params.datasets_parallel_interleave_cycle_length = None
params.datasets_sloppy_parallel_interleave = False
params.datasets_parallel_interleave_prefetch = None

dataset = datasets.create_dataset(in_dir, in_name)
model = model_config.get_model_config(model_name, in_name)()

output_shape = model.get_input_shapes(subset='train')
print(output_shape)

reader = dataset.get_input_preprocessor('default')(batch_size, output_shape, 1, dtype=model.data_type, train=False, distortions=True, resize_method='bilinear')
ds = reader.create_dataset(batch_size, 1, batch_size, dataset, 'train', False, False, None, False, None, False, None)
it = tf.compat.v1.data.make_initializable_iterator(ds)

sess.run(it.initializer)

input_list = sess.run(it.get_next())

shape1 = np.shape(input_list[0])
shape2 = np.shape(input_list[1])

shape1 = (None, shape1[1], shape1[2], shape1[3])
shape2 = (None, shape2[1])

X = tf.placeholder(model.data_type, shape=shape1)
Y_Label = tf.placeholder(tf.int32, shape=shape2)

output, extra_info = model.build_network([X, Y_Label])

Loss = tf.reduce_mean(tf.losses.sparse_softmax_cross_entropy(labels=Y_Label, logits=output))
train_step = tf.train.AdamOptimizer(0.005).minimize(Loss)

print("Start..")
sess.run(tf.global_variables_initializer())

#with tf_debug.LocalCLIDebugWrapperSession(sess) as sess:
with sess:
    for i in range(1000):
        images, labels = sess.run(it.get_next())
        sess.run(train_step, feed_dict = {X:images, Y_Label:labels})
        if i%10 == 9 :
            print("Step ", i, ":", sess.run(Loss, feed_dict = {X:images, Y_Label:labels}))
