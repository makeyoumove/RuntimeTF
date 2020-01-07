#!/bin/bash

echo "KILL IMAGENET"
pids=(`ps ax | grep python | grep imagenet | awk '{print $1}'`)
for pid in ${pids[@]} ; do
	echo $pid
        kill -9 $pid
done
