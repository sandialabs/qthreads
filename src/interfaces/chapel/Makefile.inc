# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2011 Sandia Corporation
#

lib_LIBRARIES = libqthread_chpl.a

libqthread_chpl_a_SOURCES = interfaces/chapel/src/tasks-qthreads.c
libqthread_chpl_a_CFLAGS = -I@CHPL_HOME@/runtime/include -I$(top_srcdir)/src/interfaces/chapel/include
