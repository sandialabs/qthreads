# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

SUBDIRS = basics features stress
if COMPILE_MULTINODE
SUBDIRS += multinode
endif

DIST_SUBDIRS = $(SUBDIRS) benchmarks

EXTRA_PROGRAMS = test_signal

buildbasics:
	$(MAKE) -C basics buildtests

buildfeatures:
	$(MAKE) -C features buildtests

buildstress:
	$(MAKE) -C stress buildtests

buildtests: buildbasics buildfeatures buildstress

buildextra:
	$(MAKE) -C benchmarks buildextra

buildall: buildtests buildextra

clean-local:
	$(MAKE) -C benchmarks clean

.PHONY: buildall buildtests buildextra buildbasics buildfeatures buildstress

noinst_HEADERS = argparsing.h

INCLUDES = -I$(top_srcdir)/include
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)

test_signal_SOURCES = test_signal.c
test_signal_DEPENDENCIES = $(qthreadlib)
