## Makefile.am -- process this file with automake to produce Makefile.in
noinst_LTLIBRARIES = libqtimer.la
libqtimer_la_SOURCES = qtimer.h
if USING_MACH
libqtimer_la_SOURCES += mach.c
endif
if USING_GETTIMEOFDAY
libqtimer_la_SOURCES += gettimeofday.c
endif
