
.PHONY: buildall buildtests buildextra

DIST_SUBDIRS = 

general_multinode = \
	test_uts_multinode

multinode = \
	$(general_multinode)

EXTRA_PROGRAMS = $(multinode)

EXTRA_DIST = 

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)

buildall: $(multinode)
buildextra: $(multinode)

test_uts_multinode_SOURCES = test_uts_multinode.c ../benchmarks/uts/rng/brg_sha1.c
test_uts_multinode_DEPENDENCIES = $(qthreadlib)

