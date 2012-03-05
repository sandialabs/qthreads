# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

.PHONY: buildall buildtests buildextra

TESTS = \
		test_precond_fib \
		test_task_spawn \
		test_syncvar_stream

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

test_precond_fib_SOURCES = test_precond_fib.c
test_precond_fib_DEPENDENCIES = $(qthreadlib)

test_task_spawn_SOURCES = test_task_spawn.c
test_task_spawn_DEPENDENCIES = $(qthreadlib)

test_syncvar_stream_SOURCES = test_syncvar_stream.c
test_syncvar_stream_DEPENDENCIES = $(qthreadlib)
