#!/bin/bash

## enable testing, disable perf debug output 
CFLAGS='-DQTHREAD_PERFORMANCE -DQTPERF_TESTING' ./configure --enable-debugging --disable-lazy-threadids  --enable-picky

## Enable testing AND perfdbg output
#CFLAGS='-DQTHREAD_PERFORMANCE -DQTPERF_TESTING -DPERFDBG=1' ./configure --enable-debugging --disable-lazy-threadids  --enable-picky

## Disable testing, disable perfdbg output
#CFLAGS='-DQTHREAD_PERFORMANCE' ./configure --enable-debugging --disable-lazy-threadids  --enable-picky
