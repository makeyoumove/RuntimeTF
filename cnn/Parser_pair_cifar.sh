#!/bin/bash

cifarlist=(`cat cifar_list`)
cifarlist=(`cat cifar_list`)

for batchf in ${cifarlist[@]} ; do
	batch=`echo $batchf | cut -f3-9 -d'_'`
	for cifarf in ${cifarlist[@]} ; do
		cifar=`echo $cifarf | cut -f3-9 -d'_'`
		ts=`grep elapsed data_target_${cifar}_batch_${batch}/log_target | cut -f3 -d' ' | cut -f1 -d'e'`
		min=`echo $ts | cut -f1 -d':'`
		sec=`echo $ts | cut -f2 -d':'`
		tv=`echo "$min*60+$sec" | bc -l`
#		echo $cifar $cifar $ts $tv
		echo -n $tv" "
	done
	echo " "

	for cifarf in ${cifarlist[@]} ; do
		cifar=`echo $cifarf | cut -f3-9 -d'_'`
		ts=(`grep elapsed data_target_${cifar}_batch_${batch}/log_batch | cut -f3 -d' ' | cut -f1 -d'e'`)
#		ts=`grep elapsed data_cifar10_${cifar}_cifar_${cifar}/log_cifar`
		min=`echo $ts | cut -f1 -d':'`
		sec=`echo $ts | cut -f2 -d':'`
		tv=`echo "$min*60+$sec" | bc -l`
#		echo $cifar $cifar $ts $tv
		echo -n $tv" "
	done
	echo " "
done
