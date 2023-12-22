# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

SUBDIRS = basics features stress utils
if COMPILE_MULTINODE
SUBDIRS += multinode
endif

DIST_SUBDIRS = $(SUBDIRS) multinode benchmarks

basictests:
	$(MAKE) -C basics buildtests

featuretests:
	$(MAKE) -C features buildtests

multinodetests:
	$(MAKE) -C multinode buildtests

stresstests:
	$(MAKE) -C stress buildtests

tests: basictests featuretests stresstests multinodetests

buildtests: tests

buildextra: benchmarks

benchmarks:
	$(MAKE) -C benchmarks buildextra

buildall: buildtests buildextra

.PHONY: buildall tests buildtests benchmarks buildextra basictests featuretests stresstests

noinst_HEADERS = argparsing.h

# Get the version of gcc that clang (or one of its derivatives) is using for its supporting libraries.
# This will just be empty for gcc and icc.
clang_underlying_gcc_major_version !=  clang -v 2>&1 | sed '/Selected GCC installation/!d;s/^.*\/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)$$/\1/g'
clang_underlying_gcc_minor_version !=  clang -v 2>&1 | sed '/Selected GCC installation/!d;s/^.*\/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)$$/\2/g'

AM_CPPFLAGS = -I$(top_srcdir)/include -DQTHREADS_CLANG_UNDERLYING_GCC_MAJOR_VERSION=$(clang_underlying_gcc_major_version) -DQTHREADS_CLANG_UNDERLYING_GCC_MINOR_VERSION=$(clang_underlying_gcc_minor_version)
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)
