INPUT_DIR=input
OUTPUT_DIR=output
REF_DIR=reference_output
svn info
uname -a
for SIZE in 5000
do
	for i in 1 2 3 4 5 6 7 8 9 10
	do
			for j in 125 
			do
					echo SIZE = ${SIZE} TILE = ${j}
					/usr/bin/time -v ./cholesky ${SIZE} ${j} -i ${INPUT_DIR}/${SIZE}.in -o ${OUTPUT_DIR}/${SIZE}.out
					echo
					if [ $? -ne '0' ]; then
							echo "Received error...interrupt run!"
							exit 2
					fi
					diff -q  --ignore-space-change  ${OUTPUT_DIR}/${SIZE}.out ${REF_DIR}/${SIZE}_ref.out
			done
	done
done
