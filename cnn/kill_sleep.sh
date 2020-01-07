#!/bin/bash


pids=(`ps -ef | grep sleep | grep -v kill | awk '{print $2}'`)
for pid in ${pids[@]} ; do
	kill -9 $pid
done

