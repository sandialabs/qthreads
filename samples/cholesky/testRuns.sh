INPUT_DIR=input
OUTPUT_DIR=output
REF_DIR=reference_output

#for i in 4 2 1
for SIZE in 1000 2000 #3000 4000 5000 6000 7000 9000
        do
        for j in $((${SIZE}/8)) $((${SIZE}/10)) $((${SIZE}/20)) $((${SIZE}/50))
        do
                echo SIZE = ${SIZE} TILE = ${j}
                ./bin/cholesky ${SIZE} ${j}-i ${INPUT_DIR}/${SIZE}.in -o ${OUTPUT_DIR}/${SIZE}.out
                #-th $i
                #echo Threads ${i}  Size ${SIZE} Tile ${j}
                echo
                if [ $? -ne '0' ]; then
                        echo "Received error...interrupt run!"
                        exit 2
                fi
                diff -q  ${OUTPUT_DIR}/${SIZE}.out ${REF_DIR}/${SIZE}_ref.out
        done
done
