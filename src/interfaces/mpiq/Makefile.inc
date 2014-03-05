# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2011 Sandia Corporation
#

lib_LTLIBRARIES += libqthread_mpiq.la

libqthread_mpiq_la_SOURCES = interfaces/mpiq/mpiq.c

include_HEADERS = interfaces/mpiq/mpiq.h
