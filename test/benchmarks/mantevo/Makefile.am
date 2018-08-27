SUBDIRS = hpccg miniGhost

DIST_SUBDIRS = $(SUBDIRS)

.PHONY: buildall buildextra benchmarks

mantevo_benchmarks = hpccg/hpccg miniGhost/minighost

benchmarks: buildextra
buildall: buildextra

buildextra:
	for DIR in $(SUBDIRS) ; do \
		$(MAKE) -C $$DIR buildall ; \
	done

hpccg/hpccg:
	$(MAKE) -C hpccg buildall

miniGhost/minighost:
	$(MAKE) -C miniGhost buildall

hpccg_rose/hpccg:
	$(MAKE) -C hpccg_rose buildall

AM_CPPFLAGS = -I$(top_srcdir)/include
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src libqthread.la
