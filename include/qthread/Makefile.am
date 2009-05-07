# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

qthreaddir = $(includedir)/qthread
dist_qthread_HEADERS = \
	qthread.h \
	qthread.hpp \
	allpairs.h \
	qalloc.h \
	qarray.h \
	qlfqueue.h \
	qpool.h \
	qutil.h \
	qloop.h \
	futurelib.h \
	loop_templates.hpp \
	loop_iter.hpp \
	loop.hpp
nodist_qthread_HEADERS = \
	common.h \
	qthread-int.h

if COMPILE_TARGET_SST
dist_qthread_HEADERS += qthread-sst.h
endif

distclean-local:
	$(RM) -f qthread-int.h

uninstall-local:
	test -d "$(qthreaddir)" && rmdir $(qthreaddir) || true
