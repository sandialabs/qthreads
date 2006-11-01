lib_LTLIBRARIES = libqthread.la
pkginclude_HEADERS = qthread.h qalloc.h

libqthread_la_SOURCES = qthread.c qalloc.c
libqthread_la_LDFLAGS = -version-info 1:0:0 -no-undefined
EXTRA_libqthread_la_SOURCES = context.c asm.S
libqthread_la_LIBADD = @QTHREAD_EXTRA_OBJS@
libqthread_la_DEPENDENCIES = @QTHREAD_EXTRA_OBJS@

#asm.o: asm.S
#	$(CC) $(CFLAGS) -c $^

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	find . -name CVS -exec $(RM) -r {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
