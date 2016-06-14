
QTHREADS=/home/erik/qthreads/qthreads
QTH_INC=$(QTHREADS)/include
QTH_LIB=$(QTHREADS)/src/.libs

CFLAGS_OPT=-I$(QTH_INC) -O3 -Wall -pedantic -DPERFDBG=1 -DQTPERF_TESTING -DQTHREAD_PERFORMANCE -DQTHREAD_NONLAZY_THREADIDS=1 -g $(shell pkg-config --cflags cmocka)
CFLAGS_DBG=-I$(QTH_INC) -Wall -pedantic -DPERFDBG=1 -DQTPERF_TESTING -DQTHREAD_PERFORMANCE -DQTHREAD_NONLAZY_THREADIDS=1 -g $(shell pkg-config --cflags cmocka)
CFLAGS_NOPERF=-I$(QTH_INC) -DQTHREAD_NONLAZY_THREADIDS=1 -g 
CFLAGS=${CFLAGS_DBG}

LDFLAGS=-L$(QTH_LIB) -lqthread $(shell pkg-config --libs cmocka)
TESTS=$(shell ls test_*.c)
OBJECTS=$(subst .c,.o,${TESTS})
TARGETS=$(subst test_,,$(subst .c,.test,$(shell ls test_*.c)))

all: ${TARGETS}

%.test: test_%.o $(QTH_LIB)/libqthread.la
	gcc -o $*.test test_$*.o $(LDFLAGS)

check: all
	for tst in ${TARGETS}; do echo "start $${tst}"; QT_NUM_SHEPHERDS=9 valgrind -q --leak-check=full "./$$tst" > /dev/null; echo "finish $${tst}"; done

%.o: %.c
	gcc -c -o $*.o $(CFLAGS) $*.c


clean:
	rm -f *~ *.o ${TARGETS}
