# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

AM_CPPFLAGS = $(SST_INCLUDE) -I$(top_srcdir)/include
AM_LDFLAGS = $(SST_LIBS)

SUBDIRS = osx_compat qtimer .

lib_LTLIBRARIES = libqthread.la

libqthread_la_SOURCES = \
	qalloc.c \
	qutil.c \
	qloop.c
libqthread_la_LDFLAGS = -version-info 9:0:0 -no-undefined
libqthread_la_LIBADD =
libqthread_la_DEPENDENCIES =

if COMPILE_TARGET_SST
libqthread_la_SOURCES += \
	sst/qthread-sst.c \
	sst/futurelib-sst.c
endif

if COMPILE_TARGET_UNIX
libqthread_la_SOURCES += \
	qthread.c \
	futurelib.c
endif


if QTHREAD_NEED_OSX_MAKECONTEXT
libqthread_la_LIBADD += osx_compat/libosx_compat.la
endif

if QTHREAD_NEED_QTIMER
libqthread_la_LIBADD += qtimer/libqtimer.la
endif
