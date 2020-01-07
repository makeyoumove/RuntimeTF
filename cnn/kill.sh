#!/bin/bash

echo "KILL TF"
pids=(`ps ax | grep python | grep ps | awk '{print $1}'`)
for pid in ${pids[@]} ; do
	echo $pid
        kill -9 $pid
done

echo "KILL DSTAT"
pids=(`ps ax | grep dstat | awk '{print $1}'`)
for pid in ${pids[@]} ; do
	echo $pid
	kill -9 $pid
done

echo "KILL NVIDIA-SMI"
pids=(`ps ax | grep nvidia-smi | awk '{print $1}'`)
for pid in ${pids[@]} ; do
	echo $pid
	kill -9 $pid
done

sleep 1800

echo "KILL NVPROF"
pids=(`ps ax | grep nvprof | awk '{print $1}'`)
for pid in ${pids[@]} ; do
	echo $pid
	kill -9 $pid
done
