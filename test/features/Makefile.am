# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

.PHONY: buildall buildtests buildextra

TESTS = \
		qt_loop \
		qt_loop_sinc \
		qt_loop_balance \
		qt_loop_balance_sinc \
		qutil \
		qutil_qsort \
		feb_barrier \
		qloop_utils \
		qt_loop_queue \
		qarray \
		qarray_accum \
		qpool \
		qlfqueue \
		qdqueue \
		allpairs \
		subteams \
		qt_dictionary

EXTRA_PROGRAMS = wavefront

if ENABLE_CXX_TESTS
TESTS += cxx_futurelib \
		 cxx_qt_loop \
		 cxx_qt_loop_balance
endif
if COMPILE_ROSE_EXTENSIONS
TESTS += log_barrier
endif

check_PROGRAMS = $(TESTS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = @timelimit_path@ -T 2
endif

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

qt_loop_SOURCES = qt_loop.c
qt_loop_DEPENDENCIES = $(qthreadlib)

qt_loop_sinc_SOURCES = qt_loop_sinc.c
qt_loop_sinc_DEPENDENCIES = $(qthreadlib)

qt_loop_balance_SOURCES = qt_loop_balance.c
qt_loop_balance_DEPENDENCIES = $(qthreadlib)

qt_loop_balance_sinc_SOURCES = qt_loop_balance_sinc.c
qt_loop_balance_sinc_DEPENDENCIES = $(qthreadlib)

qutil_SOURCES = qutil.c
qutil_DEPENDENCIES = $(qthreadlib)

qutil_qsort_SOURCES = qutil_qsort.c
qutil_qsort_DEPENDENCIES = $(qthreadlib)

feb_barrier_SOURCES = feb_barrier.c
feb_barrier_DEPENDENCIES = $(qthreadlib)

qloop_utils_SOURCES = qloop_utils.c
qloop_utils_DEPENDENCIES = $(qthreadlib)

qt_loop_queue_SOURCES = qt_loop_queue.c
qt_loop_queue_DEPENDENCIES = $(qthreadlib)

qpool_SOURCES = qpool.c
qpool_DEPENDENCIES = $(qthreadlib)

qarray_SOURCES = qarray.c
qarray_DEPENDENCIES = $(qthreadlib)

qarray_accum_SOURCES = qarray_accum.c
qarray_accum_DEPENDENCIES = $(qthreadlib)

qlfqueue_SOURCES = qlfqueue.c
qlfqueue_DEPENDENCIES = $(qthreadlib)

qdqueue_SOURCES = qdqueue.c
qdqueue_DEPENDENCIES = $(qthreadlib)

log_barrier_SOURCES = log_barrier.c
log_barrier_DEPENDENCIES = $(qthreadlib)

allpairs_SOURCES = allpairs.c
allpairs_DEPENDENCIES = $(qthreadlib)

subteams_SOURCES = subteams.c
subteams_DEPENDENCIES = $(qthreadlib)

cxx_futurelib_SOURCES = cxx_futurelib.cpp
cxx_futurelib_DEPENDENCIES = $(qthreadlib)

cxx_qt_loop_SOURCES = cxx_qt_loop.cpp
cxx_qt_loop_DEPENDENCIES = $(qthreadlib)

cxx_qt_loop_balance_SOURCES = cxx_qt_loop_balance.cpp
cxx_qt_loop_balance_DEPENDENCIES = $(qthreadlib)

wavefront_SOURCES = wavefront.c
wavefront_DEPENDENCIES = $(qthreadlib)
