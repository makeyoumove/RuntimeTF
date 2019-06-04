import tensorflow as tf
from tensorflow.examples.tutorials.mnist import input_data
mnist = input_data.read_data_sets('MNIST_data', one_hot = True, reshape = False)

#Image = 28 * 28 * 1
X = tf.placeholder(tf.float32, shape=[None, 28, 28, 1])
Y_Label = tf.placeholder(tf.float32, shape=[None, 10])

#1st Conv Layer = 14 * 14 * 4
Kernel1 = tf.Variable(tf.truncated_normal(shape=[4,4,1,4], stddev=0.1))
Bias1 = tf.Variable(tf.truncated_normal(shape=[4], stddev=0.1))
Conv1 = tf.nn.conv2d(X, Kernel1, strides=[1,1,1,1], padding='SAME') + Bias1
Activation1 = tf.nn.relu(Conv1)
Pool1 = tf.nn.max_pool(Activation1, ksize=[1,2,2,1], strides=[1,2,2,1], padding='SAME')

#2nd Conv Layer = 7 * 7 * 8
Kernel2 = tf.Variable(tf.truncated_normal(shape=[4,4,4,8], stddev=0.1))
Bias2 = tf.Variable(tf.truncated_normal(shape=[8], stddev=0.1))
Conv2 = tf.nn.conv2d(Pool1, Kernel2, strides=[1,1,1,1], padding='SAME') + Bias2
Activation2 = tf.nn.relu(Conv2)
Pool2 = tf.nn.max_pool(Activation2, ksize=[1,2,2,1], strides=[1,2,2,1], padding='SAME')

W1 = tf.Variable(tf.truncated_normal(shape=[8*7*7, 10]))
B1 = tf.Variable(tf.truncated_normal(shape=[10]))
Pool2_flat = tf.reshape(Pool2, [-1, 8*7*7])
OutputLayer = tf.matmul(Pool2_flat, W1) + B1

Loss = tf.reduce_mean(tf.nn.softmax_cross_entropy_with_logits(labels=Y_Label, logits=OutputLayer))
train_step = tf.train.AdamOptimizer(0.005).minimize(Loss)

correct_prediction = tf.equal(tf.argmax(OutputLayer, 1), tf.argmax(Y_Label, 1))
accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

config = tf.ConfigProto()
config.gpu_options.allow_growth = True

with tf.Session(config=config) as sess:
    print("Start..")
    sess.run(tf.global_variables_initializer())
    for i in range(100):
        trainingData, Y = mnist.train.next_batch(512)
        sess.run(train_step, feed_dict = {X:trainingData, Y_Label:Y})
        if i%100 == 0 :
            print(sess.run(accuracy, feed_dict = {X:mnist.test.images, Y_Label:mnist.test.labels}))
