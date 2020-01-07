#!/bin/bash

if [ $# -lt 1 ] ; then
	echo Usage: $0 [Split]
	exit
fi

echo Split $1 

imagenetlist=(`cat imagenet_list`)
cifarlist=(`cat cifar_list`)

for modelx in ${imagenetlist[@]} ; do
#	break
	bsize=`echo $modelx | cut -f1 -d'_'`
	iter=`echo $modelx | cut -f2 -d'_'`
	model=`echo $modelx | cut -f3-9 -d'_'`
	echo imagenet $bsize $model $iter
#	iter=50
	mkdir data_4node_imagenet_${model}_${bsize}_${iter}
#	./imagenet.sh imagenet $model $bsize $iter $1 >& data_4node_imagenet_${model}_${bsize}_${iter}/log
	./imagenet_dist.sh imagenet $model $bsize $iter $1 >& data_4node_imagenet_${model}_${bsize}_${iter}/log
	sleep 120
	exit
done

#if [ $1 -ge 2 ] ; then exit ; fi

for modelx in ${cifarlist[@]} ; do
	bsize=`echo $modelx | cut -f1 -d'_'`
	iter=`echo $modelx | cut -f2 -d'_'`
	model=`echo $modelx | cut -f3-9 -d'_'`
	echo cifar10 $bsize $model $iter
#	iter=50
	mkdir data_4node_cifar10_${model}_${bsize}_${iter}
#	./test.sh cifar10 $model $bsize $iter $1 >& data_4node_cifar10_${model}_${bsize}_${iter}/log
	./cifar_dist.sh cifar10 $model $bsize $iter $1 >& data_4node_cifar10_${model}_${bsize}_${iter}/log
	sleep 120
	exit
done
