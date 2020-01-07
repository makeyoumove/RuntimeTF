#!/bin/bash

base=1

cifar1=(`cat cifar_list`)
imagenet1=(`cat imagenet_list`)

mkdir sim_input
for cifar in ${cifar1[@]} ; do
	name=`echo $cifar | cut -f3-9 -d'_'`
	for split in 2 4 8 ; do
		paths1=(`ls -d result_${base}split_50iter_*/*_${name}_*/nvprof*`)
		paths2=(`ls -d result_${split}split_50iter_*/*_${name}_*/nvprof*`)
		for target in 1 ; do
			echo $name $split $target
			../InputGen 50 $split $target ${paths1[(${#paths1[@]}-1)]} ${paths2[(${#paths2[@]}-1)]} > sim_input/cifar_${target}_${base}_${split}_${name}
		done
	done
done

for imagenet in ${imagenet1[@]} ; do
	name=`echo $imagenet | cut -f3-9 -d'_'`
	for split in 2 4 8 ; do
		paths1=(`ls -d result_${base}split_50iter_*/*_${name}_*/nvprof*`)
		paths2=(`ls -d result_${split}split_50iter_*/*_${name}_*/nvprof*`)
		for target in 1 2 4 8 ; do
			echo $name $split $target
			../InputGen 50 $split $target ${paths1[(${#paths1[@]}-1)]} ${paths2[(${#paths2[@]}-1)]} > sim_input/imagenet_${target}_${base}_${split}_${name}
		done
	done
done
