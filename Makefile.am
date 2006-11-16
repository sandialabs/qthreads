lib_LTLIBRARIES = libqthread.la
pkginclude_HEADERS = qthread.h qalloc.h qthread-int.h

libqthread_la_SOURCES = qthread.c qalloc.c
libqthread_la_LDFLAGS = -version-info 3:0:0 -no-undefined
EXTRA_libqthread_la_SOURCES = context.c asm.S
libqthread_la_LIBADD = @QTHREAD_EXTRA_OBJS@
libqthread_la_DEPENDENCIES = @QTHREAD_EXTRA_OBJS@

TESTS = test1 test2 test3 testq
check_PROGRAMS = test1 test2 test3 testq
test1_SOURCES = test.c
test1_LDADD = -lqthread
test2_SOURCES = test2.c
test2_LDADD = -lqthread
test3_SOURCES = test3.c
test3_LDADD = -lqthread
testq_SOURCES = testq.c
testq_LDADD = -lqthread

#test2: test2.c
#	$(CC) -I. $(CFLAGS) -L.libs/libqthread.a -o $< $^

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
