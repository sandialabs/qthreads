# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

SUBDIRS = basics features stress utils
if COMPILE_MULTINODE
SUBDIRS += multinode
endif

DIST_SUBDIRS = $(SUBDIRS) benchmarks

EXTRA_PROGRAMS = test_signal

basictests:
	$(MAKE) -C basics buildtests

featuretests:
	$(MAKE) -C features buildtests

stresstests:
	$(MAKE) -C stress buildtests

tests: basictests featuretests stresstests

buildtests: tests

buildextra: benchmarks

benchmarks:
	$(MAKE) -C benchmarks buildextra

buildall: buildtests buildextra

.PHONY: buildall tests buildtests benchmarks buildextra basictests featuretests stresstests

noinst_HEADERS = argparsing.h

INCLUDES = -I$(top_srcdir)/include
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)

test_signal_SOURCES = test_signal.c
test_signal_DEPENDENCIES = $(qthreadlib)
