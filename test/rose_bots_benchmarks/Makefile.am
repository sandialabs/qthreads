# -*- Makefile -*-
#
# Copyright (c)      2011  Sandia Corporation
#

GLOBAL_COMMON = common/rose_bots_common.cc
OUTPUTDIR = $(top_builddir)/src
LIBS = $(OUTPUTDIR)/libqthread_xomp.la $(OUTPUTDIR)/libqthread.la
INCLUDES = -I$(top_srcdir)/test/rose_bots_benchmarks/common
DEFINES=-D_OPENMP -DMANUAL_CUTOFF

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@ $(DEFINES)

BOTS_TESTS = \
		fft \
		fib \
		nqueens \
		sort \
		sparselu_task \
		sparselu_forloop \
		strassen \
		health \
		alignment_task \
		alignment_forloop

buildall: $(BOTS_TESTS)
buildextra: $(BOTS_TESTS)

EXTRA_PROGRAMS = $(BOTS_TESTS)

fib_SOURCES = $(GLOBAL_COMMON) \
			  fib/rose_bots_main_fib.cc \
			  fib/rose_fib.cc
fib_DEPENDENCIES = $(LIBS)

nqueens_SOURCES = $(GLOBAL_COMMON) \
				  nqueens/rose_bots_main_nqueens.cc \
				  nqueens/rose_nqueens.cc
nqueens_DEPENDENCIES = $(LIBS)

sort_SOURCES = $(GLOBAL_COMMON) \
			   sort/rose_bots_main_sort.cc \
			   sort/rose_sort.cc
sort_DEPENDENCIES = $(LIBS)

sparselu_task_SOURCES = $(GLOBAL_COMMON) \
						sparselu/task/rose_bots_main_sparselutask.cc \
						sparselu/task/rose_sparselu_task.cc
sparselu_task_DEPENDENCIES = $(LIBS)

sparselu_forloop_SOURCES = $(GLOBAL_COMMON) \
						sparselu/forloop/rose_bots_main_sparselufor.cc \
						sparselu/forloop/rose_sparselu_for.cc
sparselu_forloop_DEPENDENCIES = $(LIBS)

strassen_SOURCES = $(GLOBAL_COMMON) \
				   strassen/rose_bots_main_strassen.cc \
				   strassen/rose_strassen.cc
strassen_DEPENDENCIES = $(LIBS)

health_SOURCES = $(GLOBAL_COMMON) \
				   health/rose_bots_main_health.cc \
				   health/rose_health.cc
health_DEPENDENCIES = $(LIBS)

alignment_forloop_SOURCES = $(GLOBAL_COMMON) \
						alignment/forloop/rose_bots_main_alignmentfor.cc \
						alignment/forloop/rose_alignment_for.cc \
						alignment/forloop/rose_sequence_alignmentfor.cc
alignment_forloop_DEPENDENCIES = $(LIBS)

alignment_task_SOURCES = $(GLOBAL_COMMON) \
						alignment/task/rose_bots_main_alignmenttask.cc \
						alignment/task/rose_alignment_task.cc \
						alignment/task/rose_sequence_alignmenttask.cc
alignment_task_DEPENDENCIES = $(LIBS)

fft_SOURCES = $(GLOBAL_COMMON) \
			  fft/rose_bots_main_fft.cc \
			  fft/rose_fft.cc
fft_DEPENDENCIES = $(LIBS)
