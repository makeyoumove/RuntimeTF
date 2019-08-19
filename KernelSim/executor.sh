#!/bin/bash

confs=(`ls conf`)

for conf in ${confs[@]} ; do
	echo $conf
	cp conf/$conf ./Conf.dat
	build/KernelSim >& res_$conf
done
