# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2011 Sandia Corporation
#

lib_LTLIBRARIES = libqthread_chpl.la

libqthread_chpl_la_SOURCES = tasks-qthreads.c
libqthread_chpl_la_CFLAGS = -I@CHPL_HOME@/runtime/include -I$(top_srcdir)/include -I$(top_srcdir)/include/qthread

include_HEADERS = tasks-qthreads.h
