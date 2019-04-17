# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2011 Sandia Corporation
#

lib_LTLIBRARIES += libqthread_mpiq.la

libqthread_mpiq_la_SOURCES = \
	interfaces/mpiq/mpiq.c \
	envariables.c

if QTHREAD_TIMER_TYPE_MACH
libqthread_mpiq_la_SOURCES += qtimer/mach.c
endif

if QTHREAD_TIMER_TYPE_GETHRTIME
libqthread_mpiq_la_SOURCES += qtimer/gethrtime.c
endif

if QTHREAD_TIMER_TYPE_GETTIMEOFDAY
libqthread_mpiq_la_SOURCES += qtimer/gettimeofday.c
endif

if QTHREAD_TIMER_TYPE_ALTIX
libqthread_mpiq_la_SOURCES += qtimer/altix.c
endif

if QTHREAD_TIMER_TYPE_GETTIME
libqthread_mpiq_la_SOURCES += qtimer/gettime.c
endif

libqthread_mpiq_la_SOURCES += alloc/base.c

include_HEADERS = interfaces/mpiq/mpiq.h
