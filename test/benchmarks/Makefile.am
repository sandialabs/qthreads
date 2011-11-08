.PHONY: buildall buildtests buildextra

EXTRA_PROGRAMS = \
	 time_qarray \
	 time_qarray_sizes \
	 time_qpool \
	 time_qlfqueue \
	 time_qdqueue \
	 time_qdqueue_sizes \
	 time_incr_bench \
	 time_incr_bench_pthread \
	 time_lul_bench \
	 time_lul_bench_pthread \
	 time_mutex_bench \
	 time_mutex_bench_pthread \
	 time_spin_bench \
	 time_spin_bench_pthread \
	 time_chain_bench \
	 time_chain_bench_pthread \
	 time_thrcrt_bench \
	 time_thrcrt_bench_pthread \
	 time_cncthr_bench \
	 time_cncthr_bench_pthread

EXTRA_DIST = \
			 pmea09/time_tbbq.cpp \
			 pmea09/time_tbbq_sizes.cpp

buildextra: $(EXTRA_PROGRAMS)

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
OUTPUTDIR = $(top_builddir)/src
QTHREADLIB = $(OUTPUTDIR)/libqthread.la

LDADD = $(QTHREADLIB)

time_qarray_SOURCES = pmea09/time_qarray.c
time_qarray_DEPENDENCIES = $(QTHREADLIB)

time_qarray_sizes_SOURCES = pmea09/time_qarray_sizes.c
time_qarray_sizes_DEPENDENCIES = $(QTHREADLIB)

time_qpool_SOURCES = pmea09/time_qpool.c
time_qpool_DEPENDENCIES = $(QTHREADLIB)

time_qlfqueue_SOURCES = pmea09/time_qlfqueue.c
time_qlfqueue_DEPENDENCIES = $(QTHREADLIB)

time_qdqueue_SOURCES = pmea09/time_qdqueue.c
time_qdqueue_DEPENDENCIES = $(QTHREADLIB)

time_qdqueue_sizes_SOURCES = pmea09/time_qdqueue_sizes.c
time_qdqueue_sizes_DEPENDENCIES = $(QTHREADLIB)

time_incr_bench_SOURCES = mtaap08/time_incr_bench.c
time_incr_bench_DEPENDENCIES = $(QTHREADLIB)

time_incr_bench_pthread_SOURCES = mtaap08/time_incr_bench_pthread.c
time_incr_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_lul_bench_SOURCES = mtaap08/time_lul_bench.c
time_lul_bench_DEPENDENCIES = $(QTHREADLIB)

time_lul_bench_pthread_SOURCES = mtaap08/time_lul_bench_pthread.c
time_lul_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_mutex_bench_SOURCES = mtaap08/time_mutex_bench.c
time_mutex_bench_DEPENDENCIES = $(QTHREADLIB)

time_mutex_bench_pthread_SOURCES = mtaap08/time_mutex_bench_pthread.c
time_mutex_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_spin_bench_SOURCES = mtaap08/time_spin_bench.c
time_spin_bench_DEPENDENCIES = $(QTHREADLIB)

time_spin_bench_pthread_SOURCES = mtaap08/time_spin_bench_pthread.c
time_spin_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_chain_bench_SOURCES = mtaap08/time_chain_bench.c
time_chain_bench_DEPENDENCIES = $(QTHREADLIB)

time_chain_bench_pthread_SOURCES = mtaap08/time_chain_bench_pthread.c
time_chain_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_thrcrt_bench_SOURCES = mtaap08/time_thrcrt_bench.c
time_thrcrt_bench_DEPENDENCIES = $(QTHREADLIB)

time_thrcrt_bench_pthread_SOURCES = mtaap08/time_thrcrt_bench_pthread.c
time_thrcrt_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

time_cncthr_bench_SOURCES = mtaap08/time_cncthr_bench.c
time_cncthr_bench_DEPENDENCIES = $(QTHREADLIB)

time_cncthr_bench_pthread_SOURCES = mtaap08/time_cncthr_bench_pthread.c
time_cncthr_bench_pthread_DEPENDENCIES = $(QTHREADLIB)

