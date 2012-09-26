#!/bin/bash
mkdir -p output
rm -f output/*.out
./cholesky_opt3 1000 50 -i "$1" -o "$2"
diff --ignore-space-change -q "$3" "$2"
