# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

.PHONY: buildall buildtests buildextra

TESTS = \
		hello_world \
		aligned_prodcons \
		hello_world_multi \
		syncvar_prodcons \
		reinitialization \
		qthread_cas \
		qthread_cacheline \
		qthread_id \
		qthread_incr qthread_fincr qthread_dincr \
		qthread_stackleft \
		qthread_migrate_to \
		qthread_disable_shepherd \
		qtimer \
		qthread_fork_precond \
		qalloc \
		arbitrary_blocking_operation \
		sinc_null \
		sinc \
		tasklocal_data \
		tasklocal_data_no_default \
		tasklocal_data_no_argcopy \
		external_fork \
		external_syncvar \
		read \
		test_teams \
		test_subteams



check_PROGRAMS = $(TESTS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = @timelimit_path@ -T 1
endif

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

hello_world_SOURCES = hello_world.c
hello_world_DEPENDENCIES = $(qthreadlib)

aligned_prodcons_SOURCES = aligned_prodcons.c
aligned_prodcons_DEPENDENCIES = $(qthreadlib)

hello_world_multi_SOURCES = hello_world_multi.c
hello_world_multi_DEPENDENCIES = $(qthreadlib)

syncvar_prodcons_SOURCES = syncvar_prodcons.c
syncvar_prodcons_DEPENDENCIES = $(qthreadlib)

reinitialization_SOURCES = reinitialization.c
reinitialization_DEPENDENCIES = $(qthreadlib)

qthread_cas_SOURCES = qthread_cas.c
qthread_cas_DEPENDENCIES = $(qthreadlib)

qthread_cacheline_SOURCES = qthread_cacheline.c
qthread_cacheline_DEPENDENCIES = $(qthreadlib)

qthread_id_SOURCES = qthread_id.c
qthread_id_DEPENDENCIES = $(qthreadlib)

qthread_incr_SOURCES = qthread_incr.c
qthread_incr_DEPENDENCIES = $(qthreadlib)

qthread_fincr_SOURCES = qthread_fincr.c
qthread_fincr_DEPENDENCIES = $(qthreadlib)

qthread_dincr_SOURCES = qthread_dincr.c
qthread_dincr_DEPENDENCIES = $(qthreadlib)

qthread_stackleft_SOURCES = qthread_stackleft.c
qthread_stackleft_DEPENDENCIES = $(qthreadlib)

qthread_migrate_to_SOURCES = qthread_migrate_to.c
qthread_migrate_to_DEPENDENCIES = $(qthreadlib)

qthread_disable_shepherd_SOURCES = qthread_disable_shepherd.c
qthread_disable_shepherd_DEPENDENCIES = $(qthreadlib)

qtimer_SOURCES = qtimer.c
qtimer_DEPENDENCIES = $(qthreadlib)

qthread_fork_precond_SOURCES = qthread_fork_precond.c
qthread_fork_precond_DEPENDENCIES = $(qthreadlib)

qalloc_SOURCES = qalloc.c
qalloc_DEPENDENCIES = $(qthreadlib)

arbitrary_blocking_operation_SOURCES = arbitrary_blocking_operation.c
arbitrary_blocking_operation_DEPENDENCIES = $(qthreadlib)

sinc_null_SOURCES = sinc_null.c
sinc_null_DEPENDENCIES = $(qthreadlib)

sinc_SOURCES = sinc.c
sinc_DEPENDENCIES = $(qthreadlib)

tasklocal_data_SOURCES = tasklocal_data.c
tasklocal_data_DEPENDENCIES = $(qthreadlib)

tasklocal_data_no_default_SOURCES = tasklocal_data_no_default.c
tasklocal_data_no_default_DEPENDENCIES = $(qthreadlib)

tasklocal_data_no_argcopy_SOURCES = tasklocal_data_no_argcopy.c
tasklocal_data_no_argcopy_DEPENDENCIES = $(qthreadlib)

external_fork_SOURCES = external_fork.c
external_fork_DEPENDENCIES = $(qthreadlib)

external_syncvar_SOURCES = external_syncvar.c
external_syncvar_DEPENDENCIES = $(qthreadlib)

read_SOURCES = read.c
read_DEPENDENCIES = $(qthreadlib)

test_teams_SOURCES = test_teams.c
test_teams_DEPENDENCIES = $(qthreadlib)

test_subteams_SOURCES = test_subteams.c
test_subteams_DEPENDENCIES = $(qthreadlib)
