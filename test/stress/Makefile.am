# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

.PHONY: buildall buildtests buildextra

TESTS = \
		syncvar_prodcons_contended \
		syncvar_stream \
		precond_fib \
		task_spawn \
		subteams_uts

check_PROGRAMS = $(TESTS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = @timelimit_path@ -T 30
endif 

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la
utils_rnglib = $(top_builddir)/test/utils/rng/librng.la

LDADD = $(qthreadlib)

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src

$(utils_rnglib):
	$(MAKE) -C $(top_builddir)/test/utils/rng

syncvar_prodcons_contended_SOURCES = syncvar_prodcons_contended.c
syncvar_prodcons_contended_DEPENDENCIES = $(qthreadlib)

precond_fib_SOURCES = precond_fib.c
precond_fib_DEPENDENCIES = $(qthreadlib)

task_spawn_SOURCES = task_spawn.c
task_spawn_DEPENDENCIES = $(qthreadlib)

syncvar_stream_SOURCES = syncvar_stream.c
syncvar_stream_DEPENDENCIES = $(qthreadlib)

subteams_uts_SOURCES = subteams_uts.c
subteams_uts_LDADD = $(LDADD) $(utils_rnglib)
