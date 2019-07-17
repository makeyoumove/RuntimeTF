#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

export CUDA_VISIBLE_DEVICES=2
#nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 test.py
#numactl -C 0-3 -N 0 pdb test.py
time numactl -C 0-3 -N 0 python3 test.py
#numactl -C 0-3 -N 0 python3 lotto.py

deactivate
