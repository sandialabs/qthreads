## Process this file with automake to produce Makefile.in
if NEED_OSX_COMPAT
MAYBE_OSX_COMPAT = osx_compat
endif
if NEED_QTIMER
MAYBE_QTIMER = qtimer
endif

SUBDIRS = man/man3 qthread $(MAYBE_OSX_COMPAT) $(MAYBE_QTIMER) .
DIST_SUBDIRS = man/man3 qthread qtimer osx_compat .

lib_LTLIBRARIES = libqthread.la

AM_CPPFLAGS = @SST_INCLUDE@
AM_LDFLAGS = @SST_LIBS@
if COMPILE_FOR_SST
libqthread_la_SOURCES = sst/qthread/qthread.h \
						sst/qthread-sst.c qalloc.c qutil.c \
						sst/futurelib-sst.c
else
libqthread_la_SOURCES = qthread.c qalloc.c qutil.c futurelib.c qloop.c
endif
libqthread_la_LDFLAGS = -version-info 9:0:0 -no-undefined
libqthread_la_LIBADD = @QTHREAD_EXTRA_OBJS@
libqthread_la_DEPENDENCIES = @QTHREAD_EXTRA_OBJS@

TESTS = test1 test2 test3 testq testloop testincr testqloop testid
check_PROGRAMS = $(TESTS)
test1_SOURCES = test.c
test1_LDADD = -lqthread
test2_SOURCES = test2.c
test2_LDADD = -lqthread
test3_SOURCES = test3.c
test3_LDADD = -lqthread
testq_SOURCES = testq.c
testq_LDADD = -lqthread
testloop_SOURCES = testloop.cpp
testloop_LDADD = -lqthread
testqloop_SOURCES = testqloop.c
testqloop_LDADD = -lqthread
testincr_SOURCES = testincr.c
testincr_LDADD = -lqthread
testid_SOURCES = test_id.c
testid_LDADD = -lqthread

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
