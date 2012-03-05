# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

.PHONY: buildall buildtests buildextra

TESTS = \
		test_qt_loop \
		test_qt_loop_sinc \
		test_qt_loop_balance \
		test_qt_loop_balance_sinc \
		test_qutil \
		test_qutil_qsort \
		test_feb_barrier \
		test_qloop_utils \
		test_queue_loop \
		test_qpool \
		test_qarray \
		test_qarray_accum \
		test_qlfqueue \
		test_qdqueue \
		test_allpairs

EXTRA_PROGRAMS = \
				 test_wavefront

if ENABLE_CXX_TESTS
TESTS += test_cxx_oldapi \
		 test_cxx_1 \
		 test_cxx_2
endif
if COMPILE_ROSE_EXTENSIONS
TESTS += test_log_barrier
endif

check_PROGRAMS = $(TESTS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = timelimit -T 30
endif

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

test_qt_loop_SOURCES = test_qt_loop.c
test_qt_loop_DEPENDENCIES = $(qthreadlib)

test_qt_loop_sinc_SOURCES = test_qt_loop_sinc.c
test_qt_loop_sinc_DEPENDENCIES = $(qthreadlib)

test_qt_loop_balance_SOURCES = test_qt_loop_balance.c
test_qt_loop_balance_DEPENDENCIES = $(qthreadlib)

test_qt_loop_balance_sinc_SOURCES = test_qt_loop_balance_sinc.c
test_qt_loop_balance_sinc_DEPENDENCIES = $(qthreadlib)

test_qutil_SOURCES = test_qutil.c
test_qutil_DEPENDENCIES = $(qthreadlib)

test_qutil_qsort_SOURCES = test_qutil_qsort.c
test_qutil_qsort_DEPENDENCIES = $(qthreadlib)

test_feb_barrier_SOURCES = test_feb_barrier.c
test_feb_barrier_DEPENDENCIES = $(qthreadlib)

test_qloop_utils_SOURCES = test_qloop_utils.c
test_qloop_utils_DEPENDENCIES = $(qthreadlib)

test_queue_loop_SOURCES = test_queue_loop.c
test_queue_loop_DEPENDENCIES = $(qthreadlib)

test_qpool_SOURCES = test_qpool.c
test_qpool_DEPENDENCIES = $(qthreadlib)

test_qarray_SOURCES = test_qarray.c
test_qarray_DEPENDENCIES = $(qthreadlib)

test_qarray_accum_SOURCES = test_qarray_accum.c
test_qarray_accum_DEPENDENCIES = $(qthreadlib)

test_qlfqueue_SOURCES = test_qlfqueue.c
test_qlfqueue_DEPENDENCIES = $(qthreadlib)

test_qdqueue_SOURCES = test_qdqueue.c
test_qdqueue_DEPENDENCIES = $(qthreadlib)

test_log_barrier_SOURCES = test_log_barrier.c
test_log_barrier_DEPENDENCIES = $(qthreadlib)

test_allpairs_SOURCES = test_allpairs.c
test_allpairs_DEPENDENCIES = $(qthreadlib)

test_cxx_oldapi_SOURCES = test_cxx_oldapi.cpp
test_cxx_oldapi_DEPENDENCIES = $(qthreadlib)

test_cxx_1_SOURCES = test_cxx_1.cpp
test_cxx_1_DEPENDENCIES = $(qthreadlib)

test_cxx_2_SOURCES = test_cxx_2.cpp
test_cxx_2_DEPENDENCIES = $(qthreadlib)

test_wavefront_SOURCES = test_wavefront.c
test_wavefront_DEPENDENCIES = $(qthreadlib)
