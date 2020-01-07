#!/bin/bash

cifars=(`cat cifar_list`)
imagenets=(`cat imagenet_list`)


for split in 2 4 8 ; do
	for imagenet in ${imagenets[@]} ; do
		rm Slowdown_Split${split}_${imagenet}
		for target in 1 2 4 8 ; do
			rm temp
			for cifar in ${cifars[@]} ; do
				echo $split $cifar $imagenet $target
				cp base_Conf.dat Conf.dat
				echo cifar_1_1_${split}_${cifar} 0 >> Conf.dat
				echo imagenet_${target}_1_${split}_${imagenet} 0 >> Conf.dat
				cp sim_input/cifar_1_1_${split}_${cifar} ./
				cp sim_input/imagenet_${target}_1_${split}_${imagenet} ./
#				./build/KernelSim
				results=(`./build/KernelSim | grep '%'`)
				a1=`echo ${results[4]} | cut -f1 -d'%'`
				a2=`echo ${results[9]} | cut -f1 -d'%'`
				echo ${results[4]} ${results[9]} >> temp
				rm cifar_1_1_${split}_${cifar} 
				rm imagenet_${target}_1_${split}_${imagenet}
			done
			vals=(`cat temp`)
			echo -n "$target " >> Slowdown_Split${split}_${imagenet}
			for ((i=0; i<${#vals[@]}; i+=2)) ; do
				echo -n "${vals[i]} " >> Slowdown_Split${split}_${imagenet}
			done
			echo "" >> Slowdown_Split${split}_${imagenet}
		done
	done
done

exit















confs=(`ls conf`)

for conf in ${confs[@]} ; do
	echo $conf
	cp conf/$conf ./Conf.dat
	build/KernelSim >& res_$conf
done
