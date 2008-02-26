if NEED_OSX_COMPAT
MAYBE_OSX_COMPAT = osx_compat
endif
if NEED_QTIMER
MAYBE_QTIMER = qtimer
endif

SUBDIRS = $(MAYBE_OSX_COMPAT) $(MAYBE_QTIMER) .
DIST_SUBDIRS = osx_compat qtimer .

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

