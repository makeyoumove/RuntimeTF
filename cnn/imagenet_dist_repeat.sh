#!/bin/bash

hosts=(ib075 ib076 ib078 ib079)

key=2224
while true ; do

for ((i=0;i<${#hosts[@]};i++)) ; do
#	ssh ${hosts[$i]} "cd RuntimeTF/cnn; ./cifar_dist_exec.sh $1 $2 $3 $4 $i >& log_${hosts[$i]}" &
	ssh ${hosts[$i]} "cd RuntimeTF/cnn; ./imagenet_dist_exec.sh $1 $2 $3 $4 $5 $i $key" &
	pid[$i]=$!
done

for ((i=0;i<${#hosts[@]};i++)) ; do
	echo Waiting ${pid[$i]}
	wait ${pid[$i]}
done
./kill_imagenet_dist.sh
sleep 15

if [ -f RUNNING ] ; then
        key=$((key+2))
	echo REPEAT
else
	break;
fi


done
