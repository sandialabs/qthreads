# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

noinst_LTLIBRARIES = libqtimer.la
libqtimer_la_SOURCES = qtimer.h

if QTHREAD_TIMER_TYPE_MACH
libqtimer_la_SOURCES += mach.c
endif

if QTHREAD_TIMER_TYPE_GETHRTIME
libqtimer_la_SOURCES += gethrtime.c
endif

if QTHREAD_TIMER_TYPE_GETTIMEOFDAY
libqtimer_la_SOURCES += gettimeofday.c
endif

