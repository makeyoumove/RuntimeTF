#!/bin/bash

X=0
bazel build --jobs 32 --config=opt //tensorflow/tools/pip_package:build_pip_package && export X=1
if [ $X -eq 0 ] ; then exit ; fi
bazel-bin/tensorflow/tools/pip_package/build_pip_package /tmp/tensorflow_pkg
source ~/RuntimeTF/tensorflow/bin/activate
source ~/.bashrc
whl=`ls /tmp/tensorflow_pkg`
echo $whl
echo y | pip3 uninstall tensorflow
pip3 install /tmp/tensorflow_pkg/$whl
deactivate
#cd ../cnn
#./test.sh &> log && vi log
