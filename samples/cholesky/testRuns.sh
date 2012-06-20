INPUT_DIR=input
OUTPUT_DIR=output
REF_DIR=reference_output
svn info
uname -a
for SIZE in 1000 2000 3000 4000 5000 6000 7000 9000
do
	for i in 1 2 3 4 5 6 7 8 9
	do
			for j in 125 #$((${SIZE}/8)) $((${SIZE}/10)) $((${SIZE}/20)) $((${SIZE}/50))
			do
					echo SIZE = ${SIZE} TILE = ${j}
					./bin/cholesky ${SIZE} ${j} -i ${INPUT_DIR}/${SIZE}.in -o ${OUTPUT_DIR}/${SIZE}.out
					#-th $i
					#echo Threads ${i}  Size ${SIZE} Tile ${j}
					echo
					if [ $? -ne '0' ]; then
							echo "Received error...interrupt run!"
							exit 2
					fi
					diff -q  --ignore-space-change  ${OUTPUT_DIR}/${SIZE}.out ${REF_DIR}/${SIZE}_ref.out
			done
	done
done
