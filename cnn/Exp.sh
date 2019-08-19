#!/bin/bash

imagenetlist=(`cat imagenet_list`)
cifarlist=(`cat cifar_list`)

for modelx in ${imagenetlist[@]} ; do
	bsize=`echo $modelx | cut -f1 -d'_'`
	model=`echo $modelx | cut -f2-9 -d'_'`
	echo imagenet $bsize $model
	./test.sh imagenet $model $bsize
	exit
done

for modelx in ${cifarlist[@]} ; do
	bsize=`echo $modelx | cut -f1 -d'_'`
	model=`echo $modelx | cut -f2-9 -d'_'`
	echo cifar10 $bsize $model
	./test.sh cifar10 $model $bsize
	exit
done
