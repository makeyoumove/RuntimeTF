#!/bin/bash


source ~/tensorflow/bin/activate
source ~/.bashrc

nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 test.py

deactivate
