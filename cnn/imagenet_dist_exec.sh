#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

if [ $# -ge 7 ] ; then
	key=$7
else
	key=2224
fi

key2=$((key+1))

export CUDA_VISIBLE_DEVICES=0

hname=`cat /etc/hostname`

#dstat -cdmn -t --output data_4node_${1}_${2}_${3}_${4}/dstat_${1}_${2}_${3}_${4}_${hname}.csv 1>> /dev/null &
#nvidia-smi dmon -s uct -o DT -i 0 >& data_4node_${1}_${2}_${3}_${4}/gpu_${1}_${2}_${3}_${4}_${hname}.csv &
#nvprof --profile-all-processes --log-file data_4node_${1}_${2}_${3}_${4}/nvprof_${1}_${2}_${3}_${4}_${hname}_%p --print-gpu-trace --csv &

date
#nvprof --log-file nvprof_gpu_trace_%p --print-gpu-trace --csv numactl -C 0-3 -N 0 python3 imagenet.py
#numactl -C 0-3 -N 0 pdb imagenet.py
CUDA_VISIBLE_DEVICES='' numactl -C 8-15 -N 0 time python3 imagenet.py --ps_hosts=ib075:${key},ib076:${key},ib078:${key},ib079:${key} --worker_hosts=ib075:${key2},ib076:${key2},ib078:${key2},ib079:${key2} --job_name=ps --task_index=$6 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5 &
sleep 5
CUDA_VISIBLE_DEVICES=0 numactl -C 8-15 -N 0 time python3 imagenet.py --ps_hosts=ib075:${key},ib076:${key},ib078:${key},ib079:${key} --worker_hosts=ib075:${key2},ib076:${key2},ib078:${key2},ib079:${key2} --job_name=worker --task_index=$6 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5
date

sleep 5
#./kill.sh
./kill_imagenet_dist.sh
#./kill_dist.sh

deactivate
