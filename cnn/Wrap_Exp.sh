#!/bin/bash

for i in 1 2 4 8 ; do
	echo $i
#	mkdir result_${i}split_191009
	mkdir result_4node_${i}split_191207
#	mkdir result_4node_pair_${i}_1_191120
#	./Exp.sh $i
	./Exp_pair_dist.sh $i 1	
#	mv data_* result_${i}split_191009
	mv data_* result_4node_${i}split_191207
#	mv data_* result_4node_pair_${i}_1_191120
done
