#!/bin/bash

imagenetlist=(`cat imagenet_list`)
cifarlist=(`cat cifar_list`)

for imagenetf in ${imagenetlist[@]} ; do
	imagenet=`echo $imagenetf | cut -f3-9 -d'_'`
	for cifarf in ${cifarlist[@]} ; do
		cifar=`echo $cifarf | cut -f3-9 -d'_'`
		ts=`grep elapsed ./${1}/data_cifar10_${cifar}_imagenet_${imagenet}/log_cifar | head -1 | cut -f3 -d' ' | cut -f1 -d'e'`
		min=`echo $ts | cut -f1 -d':'`
		sec=`echo $ts | cut -f2 -d':'`
		tv=`echo "$min*60+$sec" | bc -l`
#		echo $cifar $imagenet $ts $tv
		echo -n $tv" "
	done
	echo " "

	for cifarf in ${cifarlist[@]} ; do
		cifar=`echo $cifarf | cut -f3-9 -d'_'`
		ts=`grep elapsed ./${1}/data_cifar10_${cifar}_imagenet_${imagenet}/log_imagenet | head -1 | cut -f3 -d' ' | cut -f1 -d'e'`
#		ts=`grep elapsed data_cifar10_${cifar}_imagenet_${imagenet}/log_imagenet`
		min=`echo $ts | cut -f1 -d':'`
		sec=`echo $ts | cut -f2 -d':'`
		tv=`echo "$min*60+$sec" | bc -l`
#		echo $cifar $imagenet $ts $tv
		echo -n $tv" "
	done
	echo " "
done
