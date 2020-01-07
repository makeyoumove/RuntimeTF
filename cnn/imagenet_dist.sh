#!/bin/bash

hosts=(ib075 ib076 ib078 ib079)

for ((i=0;i<${#hosts[@]};i++)) ; do
#	ssh ${hosts[$i]} "cd RuntimeTF/cnn; ./cifar_dist_exec.sh $1 $2 $3 $4 $i >& log_${hosts[$i]}" &
	ssh ${hosts[$i]} "cd RuntimeTF/cnn; ./imagenet_dist_exec.sh $1 $2 $3 $4 $5 $i" &
	pid[$i]=$!
done

for ((i=0;i<${#hosts[@]};i++)) ; do
	echo Waiting ${pid[$i]}
	wait ${pid[$i]}
done
