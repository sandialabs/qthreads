# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

AM_CPPFLAGS = $(SST_INCLUDE) -I$(top_srcdir)/include
AM_LDFLAGS = $(SST_LIBS)
AM_CFLAGS = @SST_CFLAGS@

lib_LTLIBRARIES = libqthread.la

libqthread_la_SOURCES = \
	cacheline.c \
	qalloc.c \
	qutil.c \
	qt_feb_barrier.c \
	qloop.c

include qtimer/Makefile.inc
include ds/Makefile.inc
include patterns/Makefile.inc

# version-info fields are:
# 1. the current interface revision number (i.e. whenever arguments of existing
#    functions change, increment this number)
# 2. the revision number (the library with the highest revision number will be
#    preferred)
# 3. the age (how many previous interfaces are supported)
#
# So...
# - If you add functions, increase the interface number AND the age, and reset
#   the revision number to 0.
# - If you modify (or remove) existing functions, increase the interface number
#   and reset the revision and age numbers to 0
# - If you support all the same functions (just bugfixes or whatnot), increment
#   the revision number
libqthread_la_LDFLAGS = -version-info 12:0:1 -no-undefined

libqthread_la_LIBADD =
libqthread_la_DEPENDENCIES =

if COMPILE_CXX_HASH
libqthread_la_SOURCES += qt_hash.cpp
else
libqthread_la_SOURCES += qt_cp_hash.c
endif

if COMPILE_ROSE_EXTENSIONS
libqthread_la_SOURCES += qt_barrier.c \
						 rose_xomp.c \
						 rose_log_arrivaldetector.c
endif

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

if COMPILE_POOLED_MEMORY
libqthread_la_SOURCES += qt_mpool.c
endif

if QTHREAD_NEED_OWN_MAKECONTEXT
include osx_compat/Makefile.inc
endif
