# -*- Makefile -*-
#
# Copyright (c)      2012  Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

.PHONY: buildall buildtests buildextra

TESTS = \
		test_basic \
		test_basic_syncvar \
		test_reinit \
		test_cas \
		test_cacheline \
		test_id \
		test_incr test_fincr test_dincr \
		test_stackleft \
		test_migrate \
		test_disabled \
		test_qtimer \
		test_syncvar \
		test_precond \
		test_qalloc \
		test_user_defined_block \
		test_sinc_null \
		test_sinc \
		test_tasklocal \
		test_tasklocal_no_default \
		test_tasklocal_no_argcopy \
		test_external_fork \
		test_external_syncvar \
		test_read \
		test_teams \
		test_subteams



check_PROGRAMS = $(TESTS)

if HAVE_PROG_TIMELIMIT
TESTS_ENVIRONMENT = timelimit -T 30
endif

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/test/
qthreadlib = $(top_builddir)/src/libqthread.la

LDADD = $(qthreadlib)

buildall: $(TESTS)

buildtests: $(TESTS)

buildextra: $(TESTS)

test_basic_SOURCES = test_basic.c
test_basic_DEPENDENCIES = $(qthreadlib)

test_basic_syncvar_SOURCES = test_basic_syncvar.c
test_basic_syncvar_DEPENDENCIES = $(qthreadlib)

test_reinit_SOURCES = test_reinit.c
test_reinit_DEPENDENCIES = $(qthreadlib)

test_cas_SOURCES = test_cas.c
test_cas_DEPENDENCIES = $(qthreadlib)

test_cacheline_SOURCES = test_cacheline.c
test_cacheline_DEPENDENCIES = $(qthreadlib)

test_id_SOURCES = test_id.c
test_id_DEPENDENCIES = $(qthreadlib)

test_incr_SOURCES = test_incr.c
test_incr_DEPENDENCIES = $(qthreadlib)

test_fincr_SOURCES = test_fincr.c
test_fincr_DEPENDENCIES = $(qthreadlib)

test_dincr_SOURCES = test_dincr.c
test_dincr_DEPENDENCIES = $(qthreadlib)

test_stackleft_SOURCES = test_stackleft.c
test_stackleft_DEPENDENCIES = $(qthreadlib)

test_migrate_SOURCES = test_migrate.c
test_migrate_DEPENDENCIES = $(qthreadlib)

test_disabled_SOURCES = test_disabled.c
test_disabled_DEPENDENCIES = $(qthreadlib)

test_qtimer_SOURCES = test_qtimer.c
test_qtimer_DEPENDENCIES = $(qthreadlib)

test_syncvar_SOURCES = test_syncvar.c
test_syncvar_DEPENDENCIES = $(qthreadlib)

test_precond_SOURCES = test_precond.c
test_precond_DEPENDENCIES = $(qthreadlib)

test_qalloc_SOURCES = test_qalloc.c
test_qalloc_DEPENDENCIES = $(qthreadlib)

test_user_defined_block_SOURCES = test_user_defined_block.c
test_user_defined_block_DEPENDENCIES = $(qthreadlib)

test_sinc_null_SOURCES = test_sinc_null.c
test_sinc_null_DEPENDENCIES = $(qthreadlib)

test_sinc_SOURCES = test_sinc.c
test_sinc_DEPENDENCIES = $(qthreadlib)

test_tasklocal_SOURCES = test_tasklocal.c
test_tasklocal_DEPENDENCIES = $(qthreadlib)

test_tasklocal_no_default_SOURCES = test_tasklocal_no_default.c
test_tasklocal_no_default_DEPENDENCIES = $(qthreadlib)

test_tasklocal_no_argcopy_SOURCES = test_tasklocal_no_argcopy.c
test_tasklocal_no_argcopy_DEPENDENCIES = $(qthreadlib)

test_external_fork_SOURCES = test_external_fork.c
test_external_fork_DEPENDENCIES = $(qthreadlib)

test_external_syncvar_SOURCES = test_external_syncvar.c
test_external_syncvar_DEPENDENCIES = $(qthreadlib)

test_read_SOURCES = test_read.c
test_read_DEPENDENCIES = $(qthreadlib)

test_teams_SOURCES = test_teams.c
test_teams_DEPENDENCIES = $(qthreadlib)

test_subteams_SOURCES = test_subteams.c
test_subteams_DEPENDENCIES = $(qthreadlib)
