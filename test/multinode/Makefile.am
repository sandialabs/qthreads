
.PHONY: buildall buildtests buildextra

DIST_SUBDIRS = 

general_multinode = \
	test_init \
	hello_world \
	ring \
	uts

multinode = \
	$(general_multinode)

#EXTRA_PROGRAMS = $(multinode)

TESTS = $(multinode)
check_PROGRAMS = $(TESTS)

TESTS_ENVIRONMENT = @yod@

EXTRA_DIST = 

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la

LDADD = $(qthreadlib)

buildall: $(multinode)
buildextra: $(multinode)

test_init_SOURCES = test_init.c ../benchmarks/uts/rng/brg_sha1.c
test_init_DEPENDENCIES = $(qthreadlib)

hello_world_SOURCES = hello_world.c ../benchmarks/uts/rng/brg_sha1.c
hello_world_DEPENDENCIES = $(qthreadlib)

ring_SOURCES = ring.c ../benchmarks/uts/rng/brg_sha1.c
ring_DEPENDENCIES = $(qthreadlib)

uts_SOURCES = uts.c ../benchmarks/uts/rng/brg_sha1.c
uts_DEPENDENCIES = $(qthreadlib)

