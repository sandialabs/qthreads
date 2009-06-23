# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

if QTHREAD_NEED_OSX_MAKECONTEXT

AM_CPPFLAGS = -I$(top_srcdir)/include
noinst_LTLIBRARIES = libosx_compat.la
libosx_compat_la_SOURCES = \
	asm.S \
	context.c

endif
