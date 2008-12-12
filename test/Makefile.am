# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

TESTS = test1 test2 test3 test_qalloc \
		testincr testdincr testfincr \
		testqloop testid testloop

if ENABLE_TIMER_PROGS
TESTS += time_swap time_producerconsumer time_increments
endif

check_PROGRAMS = $(TESTS)

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
QTHREADLIB = $(OUTPUTDIR)/libqthread.la
QTIMERLIB = $(OUTPUTDIR)/qtimer/libqtimer.la
QTIMERINC = -I$(top_srcdir)/src/
CONTEXTLIB = $(OUTPUTDIR)/osx_compat/libosx_compat.la

test1_SOURCES = test1.c
test1_LDADD = $(QTHREADLIB)
test1_DEPENDENCIES = $(QTHREADLIB)

test2_SOURCES = test2.c
test2_LDADD = $(QTHREADLIB)
test2_DEPENDENCIES = $(QTHREADLIB)

test3_SOURCES = test3.c
test3_LDADD = $(QTHREADLIB)
test3_DEPENDENCIES = $(QTHREADLIB)

test_qalloc_SOURCES = testq.c
test_qalloc_LDADD = $(QTHREADLIB)
test_qalloc_DEPENDENCIES = $(QTHREADLIB)

testloop_SOURCES = testloop.cpp
testloop_LDADD = $(QTHREADLIB)
testloop_DEPENDENCIES = $(QTHREADLIB)

testqloop_SOURCES = testqloop.c
testqloop_LDADD = $(QTHREADLIB)
testqloop_DEPENDENCIES = $(QTHREADLIB)

testincr_SOURCES = testincr.c
testincr_LDADD = $(QTHREADLIB)
testincr_DEPENDENCIES = $(QTHREADLIB)

testfincr_SOURCES = testfincr.c
testfincr_LDADD = $(QTHREADLIB)
testfincr_DEPENDENCIES = $(QTHREADLIB)

testdincr_SOURCES = testdincr.c
testdincr_LDADD = $(QTHREADLIB)
testdincr_DEPENDENCIES = $(QTHREADLIB)

testid_SOURCES = test_id.c
testid_LDADD = $(QTHREADLIB)
testid_DEPENDENCIES = $(QTHREADLIB)

time_swap_SOURCES = time_swap.c
time_swap_CPPFLAGS = $(QTIMERINC)
time_swap_LDADD = $(QTIMERLIB)
time_swap_DEPENDENCIES = $(QTIMERLIB)
if QTHREAD_NEED_OSX_MAKECONTEXT
time_swap_LDADD += $(CONTEXTLIB)
time_swap_DEPENDENCIES += $(CONTEXTLIB)
endif

time_producerconsumer_SOURCES = time_producerconsumer.c
time_producerconsumer_CPPFLAGS = $(QTIMERINC)
time_producerconsumer_LDADD = $(QTIMERLIB) $(QTHREADLIB)
time_producerconsumer_DEPENDENCIES = $(QTIMERLIB) $(QTHREADLIB)

time_increments_SOURCES = time_increments.c
time_increments_CPPFLAGS = $(QTIMERINC)
time_increments_LDADD = $(QTIMERLIB) $(QTHREADLIB)
time_increments_DEPENDENCIES = $(QTIMERLIB) $(QTHREADLIB)
