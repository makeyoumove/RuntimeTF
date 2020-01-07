#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

export CUDA_VISIBLE_DEVICES=0

dstat -cdmn -t --output data_${1}_${2}_${3}_${4}/dstat_${1}_${2}_${3}_${4}.csv 1>> /dev/null &
nvidia-smi dmon -s uct -o DT -i 0 >& data_${1}_${2}_${3}_${4}/gpu_${1}_${2}_${3}_${4}.csv &
nvprof --profile-all-processes --log-file data_${1}_${2}_${3}_${4}/nvprof_${1}_${2}_${3}_${4}_%p --print-gpu-trace --csv &

date
#nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 imagenet.py
#numactl -C 0-3 -N 0 pdb imagenet.py
CUDA_VISIBLE_DEVICES='' numactl -C 8-15 -N 0 time python3 imagenet.py --ps_hosts=localhost:2224 --worker_hosts=localhost:2225 --job_name=ps --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5 &
sleep 1
CUDA_VISIBLE_DEVICES=0 numactl -C 8-15 -N 0 time python3 imagenet.py --ps_hosts=localhost:2224 --worker_hosts=localhost:2225 --job_name=worker --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5
date

sleep 5
#./kill.sh
./kill2.sh

deactivate
