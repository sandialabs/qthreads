svn info
uname -a
export QT_STACK_SIZE=$((1024*1024*2))

for OPT in {1..3} ; do
	for SCALE in 1 2 4 8 16 24 32 ; do
		for i in {1..10} ; do
			echo OPT = $OPT SCALE = $SCALE i = $i
			export QT_HWPAR=$SCALE
			/usr/bin/time -v ./filesystem_opt$OPT $((1<<15)) 1048576
		done 2>&1  | tee "log-opt$OPT-t$SCALE.txt"
	done
done
