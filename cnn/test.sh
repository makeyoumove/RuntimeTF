#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

export CUDA_VISIBLE_DEVICES=2
#nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 test.py
#numactl -C 0-3 -N 0 pdb test.py
time numactl -C 0-3 -N 0 python3 test.py --ps_hosts=localhost:2222 --worker_hosts=localhost:2223 --job_name=ps --task_index=0
#pid=$!
#numactl -C 0-3 -N 0 python3 test.py --ps_hosts=localhost:2222 --worker_hosts=localhost:2223 --job_name=worker --task_index=0
#wait $pid

deactivate
