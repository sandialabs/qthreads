lib_LTLIBRARIES = libqthread.la
pkginclude_HEADERS = qthread.h qalloc.h

libqthread_la_SOURCES = qthread.c qalloc.c
libqthread_la_LDFLAGS = -version-info 1:0:0 -no-undefined

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	find . -name CVS -exec $(RM) -r {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
