
.PHONY: buildall buildtests buildextra

DIST_SUBDIRS = rose_bots

generic_benchmarks = \
                     time_gcd \
                     time_increments \
                     time_febs \
                     time_febs_graph_test \
                     time_febs_stream_test \
                     time_producerconsumer \
                     time_syncvar_producerconsumer \
                     time_threading \
                     time_stencil_bsp \
                     time_stencil_feb \
                     time_stencil_pre \
                     time_halo_swap_all \
					 time_prodcons_comm \
                     time_qt_loops \
                     time_qt_loopaccums
thesis_benchmarks = \
                    time_allpairs \
                    time_wavefront \
                    time_qutil_qsort
pmea09_benchmarks = \
                    time_qarray \
                    time_qarray_sizes \
                    time_qpool \
                    time_qlfqueue \
                    time_qdqueue \
                    time_qdqueue_sizes
mtaap08_benchmarks = \
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
uts_benchmarks = \
				 time_uts_syncvar \
				 time_uts_aligned \
				 time_uts_sinc \
				 time_uts_donecount \
				 time_uts_donecount2 \
				 time_uts_donecount3
mt_benchmarks = \
				time_task_spawn \
				time_eager_future \
				time_fib \
				time_fib2
sc12_benchmarks = \
	spawn_sequential_qthreads \
	spawn_parallel_qthreads \
	fib_qthreads \
	uts_qthreads

if COMPILE_OMP_BENCHMARKS
generic_benchmarks += time_threading.omp
uts_benchmarks += time_uts_omp
mt_benchmarks += \
				time_omp_task_spawn \
				time_omp_eager_future \
				time_omp_fib
sc12_benchmarks += \
	spawn_sequential_omp \
	spawn_parallel_omp \
	fib_omp \
	uts_omp
endif

if COMPILE_TBB_BENCHMARKS
uts_benchmarks += time_uts_tbb
pmea09_benchmarks += \
					time_tbbq \
					time_tbbq_sizes
mt_benchmarks += \
				 time_tbb_eager_future \
				 time_tbb_fib \
				 time_tbb_task_spawn
sc12_benchmarks += \
		   spawn_sequential_tbb \
		   spawn_parallel_tbb \
		   fib_tbb \
		   uts_tbb
endif

if COMPILE_CILK_BENCHMARKS
uts_benchmarks += time_uts_cilk
mt_benchmarks += \
				 time_cilk_eager_future \
				 time_cilk_fib \
				 time_cilk_task_spawn
sc12_benchmarks += \
		   spawn_sequential_cilk \
		   spawn_parallel_cilk \
		   fib_cilk \
		   uts_cilk
endif

benchmarks = \
             $(generic_benchmarks) \
			 $(uts_benchmarks) \
             $(thesis_benchmarks) \
             $(pmea09_benchmarks) \
             $(mtaap08_benchmarks) \
	     $(mt_benchmarks) \
	     $(sc12_benchmarks)

EXTRA_PROGRAMS = $(benchmarks)
CLEANFILES = $(benchmarks)

EXTRA_DIST = \
			 pmea09/time_tbbq.cpp \
			 pmea09/time_tbbq_sizes.cpp


INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)

if COMPILE_ROSE_EXTENSIONS
buildall: $(benchmarks)
	$(MAKE) -C rose_bots buildall
buildextra: $(benchmarks)
	$(MAKE) -C rose_bots buildextra
else
buildall: $(benchmarks)
buildextra: $(benchmarks)
endif

time_allpairs_SOURCES = thesis/time_allpairs.c
time_allpairs_DEPENDENCIES = $(qthreadlib)

time_wavefront_SOURCES = thesis/time_wavefront.c
time_wavefront_DEPENDENCIES = $(qthreadlib)

time_qutil_qsort_SOURCES = thesis/time_qutil_qsort.c
time_qutil_qsort_DEPENDENCIES = $(qthreadlib)

time_qarray_SOURCES = pmea09/time_qarray.c
time_qarray_DEPENDENCIES = $(qthreadlib)

time_qarray_sizes_SOURCES = pmea09/time_qarray_sizes.c
time_qarray_sizes_DEPENDENCIES = $(qthreadlib)

time_qpool_SOURCES = pmea09/time_qpool.c
time_qpool_DEPENDENCIES = $(qthreadlib)

time_qlfqueue_SOURCES = pmea09/time_qlfqueue.c
time_qlfqueue_DEPENDENCIES = $(qthreadlib)

time_qdqueue_SOURCES = pmea09/time_qdqueue.c
time_qdqueue_DEPENDENCIES = $(qthreadlib)

time_qdqueue_sizes_SOURCES = pmea09/time_qdqueue_sizes.c
time_qdqueue_sizes_DEPENDENCIES = $(qthreadlib)

if COMPILE_TBB_BENCHMARKS
time_tbbq_SOURCES = pmea09/time_tbbq.cc
time_tbbq_DEPENDENCIES = $(qthreadlib)
time_tbbq_LDFLAGS = @TBB_LIBS@

time_tbbq_sizes_SOURCES = pmea09/time_tbbq_sizes.cc
time_tbbq_sizes_DEPENDENCIES = $(qthreadlib)
time_tbbq_sizes_LDFLAGS = @TBB_LIBS@
endif

time_incr_bench_SOURCES = mtaap08/time_incr_bench.c
time_incr_bench_DEPENDENCIES = $(qthreadlib)

time_incr_bench_pthread_SOURCES = mtaap08/time_incr_bench_pthread.c
time_incr_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_lul_bench_SOURCES = mtaap08/time_lul_bench.c
time_lul_bench_DEPENDENCIES = $(qthreadlib)

time_lul_bench_pthread_SOURCES = mtaap08/time_lul_bench_pthread.c
time_lul_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_mutex_bench_SOURCES = mtaap08/time_mutex_bench.c
time_mutex_bench_DEPENDENCIES = $(qthreadlib)

time_mutex_bench_pthread_SOURCES = mtaap08/time_mutex_bench_pthread.c
time_mutex_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_spin_bench_SOURCES = mtaap08/time_spin_bench.c
time_spin_bench_DEPENDENCIES = $(qthreadlib)

time_spin_bench_pthread_SOURCES = mtaap08/time_spin_bench_pthread.c
time_spin_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_chain_bench_SOURCES = mtaap08/time_chain_bench.c
time_chain_bench_DEPENDENCIES = $(qthreadlib)

time_chain_bench_pthread_SOURCES = mtaap08/time_chain_bench_pthread.c
time_chain_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_thrcrt_bench_SOURCES = mtaap08/time_thrcrt_bench.c
time_thrcrt_bench_DEPENDENCIES = $(qthreadlib)

time_thrcrt_bench_pthread_SOURCES = mtaap08/time_thrcrt_bench_pthread.c
time_thrcrt_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_cncthr_bench_SOURCES = mtaap08/time_cncthr_bench.c
time_cncthr_bench_DEPENDENCIES = $(qthreadlib)

time_cncthr_bench_pthread_SOURCES = mtaap08/time_cncthr_bench_pthread.c
time_cncthr_bench_pthread_DEPENDENCIES = $(qthreadlib)

time_gcd_SOURCES = generic/time_gcd.c

time_increments_SOURCES = generic/time_increments.c
time_increments_DEPENDENCIES = $(qthreadlib)

time_febs_SOURCES = generic/time_febs.c
time_febs_DEPENDENCIES = $(qthreadlib)

time_febs_graph_test_SOURCES = generic/time_febs_graph_test.c
time_febs_graph_test_DEPENDENCIES = $(qthreadlib)

time_febs_stream_test_SOURCES = generic/time_febs_stream_test.c
time_febs_stream_test_DEPENDENCIES = $(qthreadlib)

time_fib_SOURCES = mt/time_fib.c
time_fib_DEPENDENCIES = $(qthreadlib)

time_fib2_SOURCES = mt/time_fib2.c
time_fib2_DEPENDENCIES = $(qthreadlib)

time_producerconsumer_SOURCES = generic/time_producerconsumer.c
time_producerconsumer_DEPENDENCIES = $(qthreadlib)

time_syncvar_producerconsumer_SOURCES = generic/time_syncvar_producerconsumer.c
time_syncvar_producerconsumer_DEPENDENCIES = $(qthreadlib)

time_threading_SOURCES = generic/time_threading.c
time_threading_DEPENDENCIES = $(qthreadlib)

if COMPILE_OMP_BENCHMARKS
time_threading_omp_SOURCES = generic/time_threading.omp.c
time_threading_omp_DEPENDENCIES = $(qthreadlib)
time_threading_omp_CFLAGS = @OPENMP_CFLAGS@
endif

time_stencil_bsp_SOURCES = generic/time_stencil_bsp.c
time_stencil_bsp_DEPENDENCIES = $(qthreadlib)

time_stencil_feb_SOURCES = generic/time_stencil_feb.c
time_stencil_feb_DEPENDENCIES = $(qthreadlib)

time_stencil_pre_SOURCES = generic/time_stencil_pre.c
time_stencil_pre_DEPENDENCIES = $(qthreadlib)

time_halo_swap_all_SOURCES = generic/time_halo_swap_all.c
time_halo_swap_all_DEPENDENCIES = $(qthreadlib)

time_prodcons_comm_SOURCES = generic/time_prodcons_comm.c
time_prodcons_comm_DEPENDENCIES = $(qthreadlib)

time_qt_loops_SOURCES = generic/time_qt_loops.c
time_qt_loops_DEPENDENCIES = $(qthreadlib)

time_qt_loopaccums_SOURCES = generic/time_qt_loopaccums.c
time_qt_loopaccums_DEPENDENCIES = $(qthreadlib)

if COMPILE_OMP_BENCHMARKS
time_uts_omp_SOURCES = uts/time_uts_omp.c uts/rng/brg_sha1.c
time_uts_omp_DEPENDENCIES = $(qthreadlib)
time_uts_omp_CFLAGS = @OPENMP_CFLAGS@
endif

if COMPILE_TBB_BENCHMARKS
time_uts_tbb_SOURCES = uts/time_uts_tbb.cc uts/rng/brg_sha1.c
time_uts_tbb_DEPENDENCIES = $(qthreadlib)
time_uts_tbb_LDFLAGS = @TBB_LIBS@
endif

if COMPILE_CILK_BENCHMARKS
time_uts_cilk_SOURCES = uts/time_uts_cilk.c uts/rng/brg_sha1.c
time_uts_cilk_DEPENDENCIES = $(qthreadlib)
endif

time_uts_syncvar_SOURCES = uts/time_uts_syncvar.c uts/rng/brg_sha1.c
time_uts_syncvar_DEPENDENCIES = $(qthreadlib)

time_uts_aligned_SOURCES = uts/time_uts_aligned.c uts/rng/brg_sha1.c
time_uts_aligned_DEPENDENCIES = $(qthreadlib)

time_uts_sinc_SOURCES = uts/time_uts_sinc.c uts/rng/brg_sha1.c
time_uts_sinc_DEPENDENCIES = $(qthreadlib)

time_uts_donecount_SOURCES = uts/time_uts_donecount.c uts/rng/brg_sha1.c
time_uts_donecount_DEPENDENCIES = $(qthreadlib)

time_uts_donecount2_SOURCES = uts/time_uts_donecount2.c uts/rng/brg_sha1.c
time_uts_donecount2_DEPENDENCIES = $(qthreadlib)

time_uts_donecount3_SOURCES = uts/time_uts_donecount3.c uts/rng/brg_sha1.c
time_uts_donecount3_DEPENDENCIES = $(qthreadlib)

time_task_spawn_SOURCES = mt/time_task_spawn.c
time_task_spawn_DEPENDENCIES = $(qthreadlib)

time_eager_future_SOURCES = mt/time_eager_future.c
time_eager_future_DEPENDENCIES = $(qthreadlib)

if COMPILE_OMP_BENCHMARKS
time_omp_task_spawn_SOURCES = mt/time_omp_task_spawn.c
time_omp_task_spawn_DEPENDENCIES = $(qthreadlib)
time_omp_task_spawn_CFLAGS = @OPENMP_CFLAGS@

time_omp_fib_SOURCES = mt/time_omp_fib.c
time_omp_fib_DEPENDENCIES = $(qthreadlib)
time_omp_fib_CFLAGS = @OPENMP_CFLAGS@
endif

if COMPILE_TBB_BENCHMARKS
time_tbb_fib_SOURCES = mt/time_tbb_fib.cc
time_tbb_fib_DEPENDENCIES = $(qthreadlib)
time_tbb_fib_LDFLAGS = @TBB_LIBS@

time_tbb_task_spawn_SOURCES = mt/time_tbb_task_spawn.cc
time_tbb_task_spawn_DEPENDENCIES = $(qthreadlib)
time_tbb_task_spawn_LDFLAGS = @TBB_LIBS@

time_tbb_eager_future_SOURCES = mt/time_tbb_eager_future.cc
time_tbb_eager_future_DEPENDENCIES = $(qthreadlib)
time_tbb_eager_future_LDFLAGS = @TBB_LIBS@
endif

if COMPILE_CILK_BENCHMARKS
time_cilk_fib_SOURCES = mt/time_cilk_fib.c
time_cilk_fib_DEPENDENCIES = $(qthreadlib)

time_cilk_task_spawn_SOURCES = mt/time_cilk_task_spawn.c
time_cilk_task_spawn_DEPENDENCIES = $(qthreadlib)

time_cilk_eager_future_SOURCES = mt/time_cilk_eager_future.c
time_cilk_eager_future_DEPENDENCIES = $(qthreadlib)
endif

# SC12 Benchmarks

spawn_sequential_qthreads_SOURCES = sc12/spawn_sequential_qthreads.c
spawn_sequential_qthreads_DEPENDENCIES = $(qthreadlib)

spawn_parallel_qthreads_SOURCES = sc12/spawn_parallel_qthreads.c
spawn_parallel_qthreads_DEPENDENCIES = $(qthreadlib)

fib_qthreads_SOURCES = sc12/fib_qthreads.c
fib_qthreads_DEPENDENCIES = $(qthreadlib)

uts_qthreads_SOURCES = sc12/uts_qthreads.c sc12/rng/brg_sha1.c
uts_qthreads_DEPENDENCIES = $(qthreadlib)

if COMPILE_OMP_BENCHMARKS
time_omp_eager_future_SOURCES = mt/time_omp_eager_future.c
time_omp_eager_future_DEPENDENCIES = $(qthreadlib)
time_omp_eager_future_CFLAGS = @OPENMP_CFLAGS@

spawn_sequential_omp_SOURCES = sc12/spawn_sequential_omp.c
spawn_sequential_omp_DEPENDENCIES = $(qthreadlib)
spawn_sequential_omp_CFLAGS = @OPENMP_CFLAGS@

spawn_parallel_omp_SOURCES = sc12/spawn_parallel_omp.c
spawn_parallel_omp_DEPENDENCIES = $(qthreadlib)
spawn_parallel_omp_CFLAGS = @OPENMP_CFLAGS@

fib_omp_SOURCES = sc12/fib_omp.c
fib_omp_DEPENDENCIES = $(qthreadlib)
fib_omp_CFLAGS = @OPENMP_CFLAGS@

uts_omp_SOURCES = sc12/uts_omp.c sc12/rng/brg_sha1.c
uts_omp_DEPENDENCIES = $(qthreadlib)
uts_omp_CFLAGS = @OPENMP_CFLAGS@
endif

if COMPILE_TBB_BENCHMARKS
spawn_sequential_tbb_SOURCES = sc12/spawn_sequential_tbb.cc
spawn_sequential_tbb_DEPENDENCIES = $(qthreadlib)
spawn_sequential_tbb_LDFLAGS = @TBB_LIBS@

spawn_parallel_tbb_SOURCES = sc12/spawn_parallel_tbb.cc
spawn_parallel_tbb_DEPENDENCIES = $(qthreadlib)
spawn_parallel_tbb_LDFLAGS = @TBB_LIBS@

fib_tbb_SOURCES = sc12/fib_tbb.cc
fib_tbb_DEPENDENCIES = $(qthreadlib)
fib_tbb_LDFLAGS = @TBB_LIBS@

uts_tbb_SOURCES = sc12/uts_tbb.cc sc12/rng/brg_sha1.c
uts_tbb_DEPENDENCIES = $(qthreadlib)
uts_tbb_LDFLAGS = @TBB_LIBS@
endif

if COMPILE_CILK_BENCHMARKS
spawn_sequential_cilk_SOURCES = sc12/spawn_sequential_cilk.c
spawn_sequential_cilk_DEPENDENCIES = $(qthreadlib)

spawn_parallel_cilk_SOURCES = sc12/spawn_parallel_cilk.c
spawn_parallel_cilk_DEPENDENCIES = $(qthreadlib)

fib_cilk_SOURCES = sc12/fib_cilk.c
fib_cilk_DEPENDENCIES = $(qthreadlib)

uts_cilk_SOURCES = sc12/uts_cilk.c sc12/rng/brg_sha1.c
uts_cilk_DEPENDENCIES = $(qthreadlib)
endif
