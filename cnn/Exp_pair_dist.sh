#!/bin/bash

if [ $# -lt 2 ] ; then
	echo Usage: $0 [Split_imagenet] [Split_cifar]
	exit
fi

echo Split $1 $2 

imagenetlist=(`cat imagenet_list`)
cifarlist=(`cat cifar_list`)

for modelx in ${imagenetlist[@]} ; do
	bsize1=`echo $modelx | cut -f1 -d'_'`
	iter1=`echo $modelx | cut -f2 -d'_'`
	model1=`echo $modelx | cut -f3-9 -d'_'`
#	iter1=50
	for modely in ${cifarlist[@]} ; do
		bsize2=`echo $modely | cut -f1 -d'_'`
		iter2=`echo $modely | cut -f2 -d'_'`
		model2=`echo $modely | cut -f3-9 -d'_'`
		if [ -d data_cifar10_${model2}_imagenet_${model1} ] ; then
			echo data_cifar10_${model2}_imagenet_${model1} EXIST
			continue;
		fi
		echo imagenet $bsize1 $model1 $iter1 cifar10 $bsize2 $model2 $iter2
#		iter2=50
		mkdir data_cifar10_${model2}_imagenet_${model1}
		echo "RUN" > RUNNING
		dstat -cdmn -t --output data_cifar10_${model2}_imagenet_${model1}/dstat_cifar10_${model2}_imagenet_${model1}.csv 1>> /dev/null &
		nvidia-smi dmon -s uct -o DT -i 0 >& data_cifar10_${model2}_imagenet_${model1}/gpu_cifar10_${model2}_imagenet_${model1}.csv &

		./imagenet_dist_repeat.sh imagenet $model1 $bsize1 $iter1 $1 >& data_cifar10_${model2}_imagenet_${model1}/log_imagenet &
#		./imagenet_repeat.sh imagenet $model1 $bsize1 100 $1 >& data_cifar10_${model2}_imagenet_${model1}/log_imagenet &
		pid=$!
		./cifar_dist.sh cifar10 $model2 $bsize2 $iter2 $2 >& data_cifar10_${model2}_imagenet_${model1}/log_cifar
#		./test.sh cifar10 $model2 $bsize2 100 $2 >& data_cifar10_${model2}_imagenet_${model1}/log_cifar
		rm RUNNING
		echo CIFAR end
		wait $pid
		echo IMAGENET end
		./kill_dist.sh
		sleep 120
	done
	exit
done

