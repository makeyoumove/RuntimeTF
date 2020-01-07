#!/bin/bash

if [ $# -lt 1 ] ; then
	echo Usage: $0 [Split_batch]
	exit
fi

echo Split $1

cifarlist=(`cat cifar_list`)

for modelx in ${cifarlist[@]} ; do
	bsize1=`echo $modelx | cut -f1 -d'_'`
	iter1=`echo $modelx | cut -f2 -d'_'`
	model1=`echo $modelx | cut -f3-9 -d'_'`
#	iter1=50
	for modely in ${cifarlist[@]} ; do
		bsize2=`echo $modely | cut -f1 -d'_'`
		iter2=`echo $modely | cut -f2 -d'_'`
		model2=`echo $modely | cut -f3-9 -d'_'`
		echo Batch $bsize1 $model1 $iter1 Target $bsize2 $model2 $iter2
#		iter2=50
		mkdir data_target_${model2}_batch_${model1}
		echo "RUN" > RUNNING
		dstat -cdmn -t --output data_target_${model2}_batch_${model1}/dstat_target_${model2}_batch_${model1}.csv 1>> /dev/null &
		nvidia-smi dmon -s uct -o DT -i 0 >& data_target_${model2}_batch_${model1}/gpu_target_${model2}_batch_${model1}.csv &

		./cifar_repeat.sh cifar10 $model1 $bsize1 $iter1 $1 >& data_target_${model2}_batch_${model1}/log_batch &
#		./imagenet_repeat.sh imagenet $model1 $bsize1 100 $1 >& data_cifar10_${model2}_imagenet_${model1}/log_imagenet &
		pid=$!
		./test.sh cifar10 $model2 $bsize2 $iter2 >& data_target_${model2}_batch_${model1}/log_target
#		./test.sh cifar10 $model2 $bsize2 100 $2 >& data_cifar10_${model2}_imagenet_${model1}/log_cifar
		rm RUNNING
		echo TARGET end
		wait $pid
		echo Batch end
		./kill2.sh
		sleep 120
	done
done

