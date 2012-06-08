# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

.PHONY: buildall buildtests buildextra

DIST_SUBDIRS = 

general_multinode = \
	test_init \
	hello_world \
	latency \
	broadcast \
	ring \
	uts

multinode = \
	$(general_multinode)

#EXTRA_PROGRAMS = $(multinode)

TESTS = $(multinode)
check_PROGRAMS = $(TESTS)

NP := 1

if WANT_PORTALS_SHMEM_RUNTIME
TESTS_ENVIRONMENT = @yod@ -np $(NP) env QT_STACK_SIZE=65536 
endif
if WANT_MPI_RUNTIME
TESTS_ENVIRONMENT = mpirun -np $(NP) env QT_STACK_SIZE=65536 
endif
if WANT_PMI_RUNTIME
TESTS_ENVIRONMENT = srun -n $(NP) /usr/bin/env QT_STACK_SIZE=65536 
endif

EXTRA_DIST = 

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
outputdir = $(top_builddir)/src
qthreadlib = $(outputdir)/libqthread.la
utils_rnglib = $(top_builddir)/test/utils/rng/librng.la

LDADD = $(qthreadlib)

buildall: $(multinode)
buildextra: $(multinode)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src

$(utils_rng_lib):
	$(MAKE) -C $(top_builddir)/test/utils/rng

test_init_SOURCES = test_init.c
test_init_DEPENDENCIES = $(qthreadlib)

hello_world_SOURCES = hello_world.c
hello_world_DEPENDENCIES = $(qthreadlib)

latency_SOURCES = latency.c
latency_DEPENDENCIES = $(qthreadlib)

broadcast_SOURCES = broadcast.c
broadcast_DEPENDENCIES = $(qthreadlib)

ring_SOURCES = ring.c
ring_DEPENDENCIES = $(qthreadlib)

uts_SOURCES = uts.c
uts_LDADD = $(LDADD) $(utils_rnglib)
