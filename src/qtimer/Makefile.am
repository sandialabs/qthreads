# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

if QTHREAD_NEED_QTIMER

AM_CPPFLAGS = -I$(top_srcdir)/include
noinst_LTLIBRARIES = libqtimer.la
libqtimer_la_SOURCES = 

if QTHREAD_TIMER_TYPE_MACH
libqtimer_la_SOURCES += mach.c
endif

if QTHREAD_TIMER_TYPE_GETHRTIME
libqtimer_la_SOURCES += gethrtime.c
endif

if QTHREAD_TIMER_TYPE_GETTIMEOFDAY
libqtimer_la_SOURCES += gettimeofday.c
endif

if QTHREAD_TIMER_TYPE_ALTIX
libqtimer_la_SOURCES += altix.c
endif

if QTHREAD_TIMER_TYPE_GETTIME
libqtimer_la_SOURCES += gettime.c
endif

endif
