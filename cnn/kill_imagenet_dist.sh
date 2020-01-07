#!/bin/bash

hosts=(dumbo075 dumbo076 dumbo078 dumbo079)

for host in ${hosts[@]} ; do
#	ssh $host "cd RuntimeTF/cnn; ./kill2.sh" &
	ssh $host "cd RuntimeTF/cnn; ./kill_imagenet.sh" &
	pid=$!
done

wait $pid
sleep 5
