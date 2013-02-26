# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2010 Sandia Corporation
#

libqthread_la_SOURCES += \
			 ds/qarray.c \
			 ds/qdqueue.c \
			 ds/qlfqueue.c \
			 ds/qswsrqueue.c \
			 ds/qpool.c 

if COMPILE_TARGET_UNIX
libqthread_la_SOURCES += ds/dictionary/dictionary_@with_dict@.c
endif

EXTRA_DIST += \
			 ds/dictionary/dictionary_shavit.c \
			 ds/dictionary/dictionary_trie.c \
			 ds/dictionary/dictionary_simple.c
