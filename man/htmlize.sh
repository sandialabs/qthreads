#!/bin/bash
for F in man3/*.3 ; do
	rman -f html -r '%s.html' -S $F > ${F%.3}.html || (echo $F && break)
done
