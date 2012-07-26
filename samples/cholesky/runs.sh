INPUT_DIR=input
OUTPUT_DIR=output
REF_DIR=reference_output
export QT_STACK_SIZE=$((1024*1024*2))
svn info
uname -a
for OPT in {1..3} ; do 
	for SCALE in 1 2 4 8 16 24 32 ; do
		for SIZE in 4000 ; do 
			for i in {1..10} ; do 
				for j in 125 ; do
					echo SIZE = ${SIZE} TILE = ${j} SCALE = $SCALE i = $i
					echo /usr/bin/time -v ./cholesky_opt${OPT} ${SIZE} ${j} -i ${INPUT_DIR}/${SIZE}.in
					export QT_HWPAR=$SCALE
					/usr/bin/time -v ./cholesky_opt${OPT} ${SIZE} ${j} -i ${INPUT_DIR}/${SIZE}.in
					echo
					if [ $? -ne '0' ]; then
						echo "Received error...interrupt run!"
						exit 2
					fi
					#diff -q  --ignore-space-change  ${OUTPUT_DIR}/${SIZE}.out ${REF_DIR}/${SIZE}_ref.out
				done
			done
		done 2>&1 | tee "log-opt$OPT-t$SCALE.txt"
	done
done
