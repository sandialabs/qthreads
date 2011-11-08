# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

BASIC_TESTS = test_basic \
			  test_basic_syncvar \
			  test_reinit \
			  test_cas \
			  test_cacheline \
			  test_id \
			  test_incr test_fincr test_dincr \
			  test_stackleft \
			  test_external_fork \
			  test_external_syncvar \
			  test_migrate \
			  test_disabled \
			  test_qtimer \
			  test_syncvar \
			  test_syncvar2 \
			  test_precond \
			  test_qalloc \
			  test_user_defined_block \
			  test_sinc \
			  test_tasklocal \
			  test_tasklocal_no_default \
			  test_tasklocal_no_argcopy \
			  test_read

COMPLEX_TESTS = test_qt_loop \
				test_qt_loop_sinc \
				test_qt_loop_balance \
				test_qt_loop_balance_sinc \
				test_qutil \
				test_qutil_qsort \
				test_feb_barrier \
				test_precond_fib \
				testqloop \
				test_queue_loop \
				test_qpool \
				test_qarray \
				test_qarray_accum \
				test_qlfqueue \
				test_qdqueue \
				test_allpairs

if ENABLE_CXX_TESTS
BASIC_TESTS += test_cpp
COMPLEX_TESTS += testloop
endif

SUBDIRS = 
DIST_SUBDIRS = benchmarks

if COMPILE_ROSE_EXTENSIONS
SUBDIRS += rose_bots_benchmarks
endif

TESTS =  $(BASIC_TESTS) $(COMPLEX_TESTS)
EXTRA_PROGRAMS = 

UNBUILDABLE = test_wavefront

if COMPILE_ROSE_EXTENSIONS
TESTS += test_log_barrier
endif

if WANT_MULTINODE
TESTS += test_multinode_init
endif

BENCHMARKS = \
	 time_increments \
	 time_febs \
	 time_qarray \
	 time_qarray_sizes \
	 time_producerconsumer \
	 time_syncvar_producerconsumer \
	 time_qpool \
	 time_qlfqueue \
	 time_qdqueue \
	 time_qdqueue_sizes \
	 time_qutil_qsort \
	 time_allpairs \
	 time_wavefront \
	 time_threading \
	 time_stencil_bsp \
	 time_stencil_feb \
	 time_stencil_pre \
	 time_halo_swap_all \
	 time_gcd \
	 test_cpp_interface \
	 time_qt_loops \
	 time_qt_loopaccums

UNBUILDABLE += time_uts_syncvar time_uts_sinc time_uts_donecount
EXTRA_PROGRAMS += $(UNBUILDABLE) $(BENCHMARKS)

CLEANFILES += $(UNBUILDABLE) $(BENCHMARKS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = timelimit -T 30
endif

buildtests: $(TESTS)
if COMPILE_ROSE_EXTENSIONS
buildall: $(TESTS)
	$(MAKE) -C benchmarks buildall
	$(MAKE) -C rose_bots_benchmarks buildall
buildextra: $(BENCHMARKS)
	$(MAKE) -C benchmarks buildextra
	$(MAKE) -C rose_bots_benchmarks buildextra
else
buildall: $(TESTS) $(BENCHMARKS)
	$(MAKE) -C benchmarks buildall
buildextra: $(BENCHMARKS)
	$(MAKE) -C benchmarks buildextra
endif

.PHONY: buildall buildtests buildextra

check_PROGRAMS = $(TESTS)

noinst_HEADERS = argparsing.h

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
QTHREADLIB = $(OUTPUTDIR)/libqthread.la

LDADD = $(QTHREADLIB)

test_qt_loop_SOURCES = test_qt_loop.c
test_qt_loop_DEPENDENCIES = $(QTHREADLIB)

test_qt_loop_sinc_SOURCES = test_qt_loop_sinc.c
test_qt_loop_sinc_DEPENDENCIES = $(QTHREADLIB)

test_qt_loop_balance_SOURCES = test_qt_loop_balance.c
test_qt_loop_balance_DEPENDENCIES = $(QTHREADLIB)

test_qt_loop_balance_sinc_SOURCES = test_qt_loop_balance_sinc.c
test_qt_loop_balance_sinc_DEPENDENCIES = $(QTHREADLIB)

test_qutil_SOURCES = test_qutil.c
test_qutil_DEPENDENCIES = $(QTHREADLIB)

test_stackleft_SOURCES = test_stackleft.c
test_stackleft_DEPENDENCIES = $(QTHREADLIB)

test_basic_SOURCES = test_basic.c
test_basic_DEPENDENCIES = $(QTHREADLIB)

test_tasklocal_SOURCES = test_tasklocal.c
test_tasklocal_DEPENDENCIES = $(QTHREADLIB)

test_tasklocal_no_default_SOURCES = test_tasklocal_no_default.c
test_tasklocal_no_default_DEPENDENCIES = $(QTHREADLIB)

test_tasklocal_no_argcopy_SOURCES = test_tasklocal_no_argcopy.c
test_tasklocal_no_argcopy_DEPENDENCIES = $(QTHREADLIB)

test_precond_SOURCES = test_precond.c
test_precond_DEPENDENCIES = $(QTHREADLIB)

test_precond_fib_SOURCES = test_precond_fib.c
test_precond_fib_DEPENDENCIES = $(QTHREADLIB)

test_reinit_SOURCES = test_reinit.c
test_reinit_DEPENDENCIES = $(QTHREADLIB)

test_qalloc_SOURCES = testq.c
test_qalloc_DEPENDENCIES = $(QTHREADLIB)

testloop_SOURCES = testloop.cpp
testloop_DEPENDENCIES = $(QTHREADLIB)

test_cpp_SOURCES = test_cpp.cpp
test_cpp_DEPENDENCIES = $(QTHREADLIB)

test_cpp_interface_SOURCES = test_cpp_interface.cpp
test_cpp_interface_DEPENDENCIES = $(QTHREADLIB)

testqloop_SOURCES = testqloop.c
testqloop_DEPENDENCIES = $(QTHREADLIB)

test_queue_loop_SOURCES = test_queue_loop.c
test_queue_loop_DEPENDENCIES = $(QTHREADLIB)

test_incr_SOURCES = testincr.c
test_incr_DEPENDENCIES = $(QTHREADLIB)

test_fincr_SOURCES = testfincr.c
test_fincr_DEPENDENCIES = $(QTHREADLIB)

test_dincr_SOURCES = testdincr.c
test_dincr_DEPENDENCIES = $(QTHREADLIB)

test_disabled_SOURCES = test_disabled.c
test_disabled_DEPENDENCIES = $(QTHREADLIB)

test_cas_SOURCES = test_cas.c
test_cas_DEPENDENCIES = $(QTHREADLIB)

test_cacheline_SOURCES = test_cacheline.c
test_cacheline_DEPENDENCIES = $(QTHREADLIB)

test_id_SOURCES = test_id.c
test_id_DEPENDENCIES = $(QTHREADLIB)

test_allpairs_SOURCES = test_allpairs.c
test_allpairs_DEPENDENCIES = $(QTHREADLIB)

test_wavefront_SOURCES = test_wavefront.c
test_wavefront_DEPENDENCIES = $(QTHREADLIB)

test_migrate_SOURCES = test_migrate.c
test_migrate_DEPENDENCIES = $(QTHREADLIB)

test_qutil_qsort_SOURCES = test_qutil_qsort.c
test_qutil_qsort_DEPENDENCIES = $(QTHREADLIB)

time_qutil_qsort_SOURCES = time_qutil_qsort.c
time_qutil_qsort_DEPENDENCIES = $(QTHREADLIB)

test_qarray_SOURCES = test_qarray.c
test_qarray_DEPENDENCIES = $(QTHREADLIB)

test_qarray_accum_SOURCES = test_qarray_accum.c
test_qarray_accum_DEPENDENCIES = $(QTHREADLIB)

test_qlfqueue_SOURCES = test_qlfqueue.c
test_qlfqueue_DEPENDENCIES = $(QTHREADLIB)

test_qdqueue_SOURCES = test_qdqueue.c
test_qdqueue_DEPENDENCIES = $(QTHREADLIB)

test_qpool_SOURCES = test_qpool.c
test_qpool_DEPENDENCIES = $(QTHREADLIB)

time_gcd_SOURCES = time_gcd.c

time_threading_SOURCES = time_threading.c
time_threading_DEPENDENCIES = $(QTHREADLIB)

time_producerconsumer_SOURCES = time_producerconsumer.c
time_producerconsumer_DEPENDENCIES = $(QTHREADLIB)

time_syncvar_producerconsumer_SOURCES = time_syncvar_producerconsumer.c
time_syncvar_producerconsumer_DEPENDENCIES = $(QTHREADLIB)

time_increments_SOURCES = time_increments.c
time_increments_DEPENDENCIES = $(QTHREADLIB)

time_febs_SOURCES = time_febs.c
time_febs_DEPENDENCIES = $(QTHREADLIB)

time_qarray_SOURCES = time_qarray.c
time_qarray_DEPENDENCIES = $(QTHREADLIB)

time_qarray_sizes_SOURCES = time_qarray_sizes.c
time_qarray_sizes_DEPENDENCIES = $(QTHREADLIB)

time_qpool_SOURCES = time_qpool.c
time_qpool_DEPENDENCIES = $(QTHREADLIB)

time_qlfqueue_SOURCES = time_qlfqueue.c
time_qlfqueue_DEPENDENCIES = $(QTHREADLIB)

time_qdqueue_SOURCES = time_qdqueue.c
time_qdqueue_DEPENDENCIES = $(QTHREADLIB)

time_allpairs_SOURCES = time_allpairs.c
time_allpairs_DEPENDENCIES = $(QTHREADLIB)

time_wavefront_SOURCES = time_wavefront.c
time_wavefront_DEPENDENCIES = $(QTHREADLIB)

test_feb_barrier_SOURCES = test_feb_barrier.c
test_feb_barrier_DEPENDENCIES = $(QTHREADLIB)

test_log_barrier_SOURCES = test_log_barrier.c
test_log_barrier_DEPENDENCIES = $(QTHREADLIB)

test_external_fork_SOURCES = test_external_fork.c
test_external_fork_DEPENDENCIES = $(QTHREADLIB)

test_external_syncvar_SOURCES = test_external_syncvar.c
test_external_syncvar_DEPENDENCIES = $(QTHREADLIB)

test_read_SOURCES = test_read.c
test_read_DEPENDENCIES = $(QTHEADLIB)

time_stencil_bsp_SOURCES = time_stencil_bsp.c
time_stencil_bsp_DEPENDENCIES = $(QTHREADLIB)

time_stencil_feb_SOURCES = time_stencil_feb.c
time_stencil_feb_DEPENDENCIES = $(QTHREADLIB)

time_stencil_pre_SOURCES = time_stencil_pre.c
time_stencil_pre_DEPENDENCIES = $(QTHREADLIB)

time_halo_swap_all_SOURCES = time_halo_swap_all.c
time_halo_swap_all_DEPENDENCIES = $(QTHREADLIB)

time_qt_loops_SOURCES = time_qt_loops.c
time_qt_loops_DEPENDENCIES = $(QTHREADLIB)

time_qt_loopaccums_SOURCES = time_qt_loopaccums.c
time_qt_loopaccums_DEPENDENCIES = $(QTHREADLIB)

time_uts_syncvar_SOURCES = time_uts_syncvar.c rng/brg_sha1.c
time_uts_syncvar_DEPENDENCIES = $(QTHREADLIB)

time_uts_sinc_SOURCES = time_uts_sinc.c rng/brg_sha1.c
time_uts_sinc_DEPENDENCIES = $(QTHREADLIB)

time_uts_donecount_SOURCES = time_uts_donecount.c rng/brg_sha1.c
time_uts_donecount_DEPENDENCIES = $(QTHREADLIB)

test_sinc_SOURCES = test_sinc.c
test_sinc_DEPENDENCIES = $(QTHREADLIB)

test_multinode_init_SOURCES = test_multinode_init.c
test_multinode_init_DEPENDENCIES = $(QTHREADLIB)
