## Process this file with automake to produce Makefile.in

EXTRA_DIST = autogen.sh

SUBDIRS = man include test src

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
