#!/bin/bash
svn info
uname -a
export QT_STACK_SIZE=$((1024*1024*2))
for OPT in {1..3} ; do
	for SCALE in 1 2 4 8 16 24 32 ; do
		for SIZE in 2048 ; do
			for i in {1..10} ; do
				export QT_HWPAR=$SCALE
				echo OPT = $OPT SCALE = $SCALE i = $i
				/usr/bin/time -v ./matrixinvert_opt$OPT $SIZE
				echo
				if [ $? -ne '0' ]; then
					echo "Received error...interrupt run!"
					exit 2
				fi
			done
		done 2>&1 | tee "log-opt$OPT-t$SCALE.txt"
	done
done
