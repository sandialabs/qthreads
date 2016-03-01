# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

.PHONY: buildall buildtests buildextra

TESTS = \
		issue3\
		issue11

check_PROGRAMS = $(TESTS)

TESTS_ENVIRONMENT =

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT += @timelimit_path@ -T 2
endif

if WANT_SINGLE_WORKER_SCHEDULER
TESTS_ENVIRONMENT += env QT_NUM_SHEPHERDS=2 QT_NUM_WORKERS_PER_SHEPHERD=1
endif

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

LDADD = $(qthreadlib)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src libqthread.la

issue3_SOURCES = issue3.c

issue11_SOURCES = issue11.c
