#!/bin/bash

source ~/RuntimeTF/tensorflow/bin/activate
#source ~/tensorflow/bin/activate
source ~/.bashrc

export CUDA_VISIBLE_DEVICES=0

i=2224
j=2225
date
while true; do
#numactl -C 0-3 -N 0 pdb imagenet.py
	CUDA_VISIBLE_DEVICES='' numactl -C 0-7 -N 0 time python3 test.py --ps_hosts=localhost:$i --worker_hosts=localhost:$j --job_name=ps --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5 &
	sleep 1
	CUDA_VISIBLE_DEVICES=0 numactl -C 0-7 -N 0 time python3 test.py --ps_hosts=localhost:$i --worker_hosts=localhost:$j --job_name=worker --task_index=0 --data_name=$1 --model_name=$2 --batch_size=$3 --iter=$4 --split=$5

	sleep 5

	pids=(`ps ax | grep python | grep split | awk '{print $1}'`)
	for pid in ${pids[@]} ; do
		echo $pid
		kill -9 $pid
	done

if [ -f RUNNING ] ; then 
	i=$((i+2))
	j=$((j+2))
	echo REPEAT $i $j
else
	break;
fi
done


deactivate
