#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

export CUDA_VISIBLE_DEVICES=0

hname=`cat /etc/hostname`

#dstat -cdmn -t --output data_4node_${1}_${2}_${3}_${4}/dstat_${1}_${2}_${3}_${4}_${hname}.csv 1>> /dev/null &
#nvidia-smi dmon -s uct -o DT -i 0 >& data_4node_${1}_${2}_${3}_${4}/gpu_${1}_${2}_${3}_${4}_${hname}.csv &
#nvprof --profile-all-processes --log-file data_4node_${1}_${2}_${3}_${4}/nvprof_${1}_${2}_${3}_${4}_${hname}_%p --print-gpu-trace --csv &

date
#nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 test.py
#numactl -C 0-3 -N 0 pdb test.py
#CUDA_VISIBLE_DEVICES='' numactl -C 0-7 -N 0 time python3 test.py --ps_hosts=localhost:2222 --worker_hosts=localhost:2223 --job_name=ps --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5 &
CUDA_VISIBLE_DEVICES='' numactl -C 8-15 -N 0 time python3 test.py --ps_hosts=ib075:2222,ib076:2222,ib078:2222,ib079:2222 --worker_hosts=ib075:2223,ib076:2223,ib078:2223,ib079:2223 --job_name=ps --task_index=$5 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 &
sleep 5
#CUDA_VISIBLE_DEVICES=0 numactl -C 0-7 -N 0 time python3 test.py --ps_hosts=localhost:2222 --worker_hosts=localhost:2223 --job_name=worker --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5
CUDA_VISIBLE_DEVICES=0 numactl -C 8-15 -N 0 time python3 test.py --ps_hosts=ib075:2222,ib076:2222,ib078:2222,ib079:2222 --worker_hosts=ib075:2223,ib076:2223,ib078:2223,ib079:2223 --job_name=worker --task_index=$5 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4
date

sleep 5
#./kill.sh
#./kill2.sh
./kill_dist.sh

deactivate
