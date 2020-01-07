#!/bin/bash

if [ $# -lt 1 ] ; then
	echo Usage : $0 [Dir] [iter]
	exit
fi

split=`echo $1 | cut -f3 -d'_' | cut -f1 -d's'`
drs=(`ls $1/`)

for dr in ${drs[@]} ; do
	dataset=`echo $dr | cut -f3 -d'_'`
	model=`echo $dr | cut -f4 -d'_'`
	bsize=`echo $dr | cut -f5 -d'_'`
	iter=`echo $dr | cut -f6 -d'_'`
	ds=`ls $1/$dr/dstat*`
	gp=`ls $1/$dr/gpu*`
#	nv=(`ls $1/$dr/nvprof*`)
#	nv=${nv[${#nv[@]}-1]}
	dss=(`~/RuntimeTF/DSTATparser $ds`)
	gps=(`~/RuntimeTF/GPUparser $gp`)
	echo $dataset $model $bsize $iter $split ${dss[@]} ${gps[1]} ${gps[2]} ${gps[3]} ${gps[4]} ${gps[5]}
#	~/RuntimeTF/InputGen $2 $nv
done
