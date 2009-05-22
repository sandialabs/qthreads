# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

TESTS = test1 test2 test3 test_qalloc \
		testincr testdincr testfincr \
		testqloop testid \
		test_internal_mod \
		test_allpairs \
		test_wavefront \
		test_migrate \
		test_qarray \
		test_qpool \
		test_qlfqueue \
		test_qdqueue \
		test_qutil_qsort

if ENABLE_CXX_TESTS
TESTS += testloop
endif

if ENABLE_TIMER_PROGS
TESTS += time_swap time_producerconsumer time_increments \
	 time_qarray \
	 time_qarray_sizes \
	 time_qpool \
	 time_qlfqueue \
	 time_qdqueue \
	 time_qdqueue_sizes \
	 time_gcd
endif

buildall: $(TESTS)
.PHONY: buildall

check_PROGRAMS = $(TESTS)

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
QTHREADLIB = $(OUTPUTDIR)/libqthread.la
QTIMERLIB = $(OUTPUTDIR)/qtimer/libqtimer.la
CONTEXTLIB = $(OUTPUTDIR)/osx_compat/libosx_compat.la

LDADD = $(QTHREADLIB)

test1_SOURCES = test1.c
test1_DEPENDENCIES = $(QTHREADLIB)

test2_SOURCES = test2.c
test2_DEPENDENCIES = $(QTHREADLIB)

test3_SOURCES = test3.c
test3_DEPENDENCIES = $(QTHREADLIB)

test_qalloc_SOURCES = testq.c
test_qalloc_DEPENDENCIES = $(QTHREADLIB)

testloop_SOURCES = testloop.cpp
testloop_DEPENDENCIES = $(QTHREADLIB)

testqloop_SOURCES = testqloop.c
testqloop_DEPENDENCIES = $(QTHREADLIB)

testincr_SOURCES = testincr.c
testincr_DEPENDENCIES = $(QTHREADLIB)

testfincr_SOURCES = testfincr.c
testfincr_DEPENDENCIES = $(QTHREADLIB)

testdincr_SOURCES = testdincr.c
testdincr_DEPENDENCIES = $(QTHREADLIB)

testid_SOURCES = test_id.c
testid_DEPENDENCIES = $(QTHREADLIB)

test_internal_mod_SOURCES = test_internal_mod.c
test_internal_mod_DEPENDENCIES = $(QTHREADLIB)

test_allpairs_SOURCES = test_allpairs.c
test_allpairs_DEPENDENCIES = $(QTHREADLIB)

test_wavefront_SOURCES = test_wavefront.c
test_wavefront_DEPENDENCIES = $(QTHREADLIB)

test_migrate_SOURCES = test_migrate.c
test_migrate_DEPENDENCIES = $(QTHREADLIB)

test_qutil_qsort_SOURCES = test_qutil_qsort.c
test_qutil_qsort_DEPENDENCIES = $(QTHREADLIB)

test_qarray_SOURCES = test_qarray.c
test_qarray_DEPENDENCIES = $(QTHREADLIB)

test_qlfqueue_SOURCES = test_qlfqueue.c
test_qlfqueue_DEPENDENCIES = $(QTHREADLIB)

test_qdqueue_SOURCES = test_qdqueue.c
test_qdqueue_DEPENDENCIES = $(QTHREADLIB)

test_qpool_SOURCES = test_qpool.c
test_qpool_DEPENDENCIES = $(QTHREADLIB)

time_gcd_SOURCES = time_gcd.c
time_gcd_LDADD = $(QTIMERLIB)
time_gcd_DEPENDENCIES = $(QTIMERLIB)

time_swap_SOURCES = time_swap.c
time_swap_CPPFLAGS = -I$(top_srcdir)/src/ # for osx_compat
time_swap_LDADD = $(QTIMERLIB)
time_swap_DEPENDENCIES = $(QTIMERLIB)
if QTHREAD_NEED_OSX_MAKECONTEXT
time_swap_LDADD += $(CONTEXTLIB)
time_swap_DEPENDENCIES += $(CONTEXTLIB)
endif

time_producerconsumer_SOURCES = time_producerconsumer.c
time_producerconsumer_LDADD = $(QTHREADLIB)
time_producerconsumer_DEPENDENCIES = $(QTHREADLIB)

time_increments_SOURCES = time_increments.c
time_increments_LDADD = $(QTHREADLIB)
time_increments_DEPENDENCIES = $(QTHREADLIB)

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
