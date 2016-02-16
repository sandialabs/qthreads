#!/bin/bash

CFLAGS='-DQTHREAD_PERFORMANCE -DQTPERF_TESTING' ./configure --enable-debugging --disable-lazy-threadids  --enable-picky
#CFLAGS='-DQTHREAD_PERFORMANCE' ./configure --enable-debugging --disable-lazy-threadids  --enable-picky
