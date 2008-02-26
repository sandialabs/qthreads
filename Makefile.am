## Process this file with automake to produce Makefile.in
if NEED_OSX_COMPAT
MAYBE_OSX_COMPAT = osx_compat
endif
if NEED_QTIMER
MAYBE_QTIMER = qtimer
endif

SUBDIRS = man/man3 qthread $(MAYBE_OSX_COMPAT) $(MAYBE_QTIMER) test .
DIST_SUBDIRS = man/man3 qthread qtimer osx_compat test .

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

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
