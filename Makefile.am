## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = check-news dist-bzip2

SUBDIRS = man/man3 include/qthread test src

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
