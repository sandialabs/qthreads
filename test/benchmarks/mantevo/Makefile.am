SUBDIRS = hpccg hpccg_openmp

.PHONY: buildall buildextra benchmarks

benchmarks: buildextra
buildall: buildextra

mantevo_benchmarks = hpccg/hpccg hpccg_openmp/hpccg

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
