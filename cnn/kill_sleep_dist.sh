#!/bin/bash

hosts=(dumbo075 dumbo076 dumbo078 dumbo079)

for host in ${hosts[@]} ; do
	echo $host
	ssh $host "RuntimeTF/cnn/kill_sleep.sh"
done
