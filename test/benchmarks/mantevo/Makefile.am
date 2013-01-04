SUBDIRS = hpccg

DIST_SUBDIRS = $(SUBDIRS) hpccg_openmp

.PHONY: buildall buildextra benchmarks

mantevo_benchmarks = hpccg/hpccg

if COMPILE_ROSE_EXTENSIONS
SUBDIRS += hpccg_openmp
mantevo_benchmarks += hpccg_openmp/hpccg
endif

benchmarks: buildextra
buildall: buildextra

buildextra:
	for DIR in $(SUBDIRS) ; do \
		$(MAKE) -C $$DIR $$DIR ; \
	done

hpccg/hpccg:
	$(MAKE) -C hpccg buildall

hpccg_openmp/hpccg:
	$(MAKE) -C hpccg_openmp buildall

AM_CPPFLAGS = -I$(top_srcdir)/include
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src libqthread.la

clean-local:
	for DIR in $(SUBDIRS) ; do \
	$(MAKE) -C $$DIR clean ; \
	done
