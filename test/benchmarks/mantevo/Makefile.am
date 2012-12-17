SUBDIRS = hpccg

DIST_SUBDIRS = $(SUBDIRS) hpccg_openmp

.PHONY: buildall buildextra benchmarks

benchmarks: buildextra
buildall: buildextra

mantevo_benchmarks = hpccg/hpccg

if COMPILE_ROSE_EXTENSIONS
mantevo_benchmarks += hpccg_openmp/hpccg
endif

buildextra: $(mantevo_benchmarks)

hpccg/hpccg:
	$(MAKE) -C hpccg buildall

hpccg_openmp/hpccg:
	$(MAKE) -C hpccg_openmp buildall

INCLUDES = -I$(top_srcdir)/include
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src libqthread.la
