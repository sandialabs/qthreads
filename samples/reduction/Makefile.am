AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testreduction runreduction

TESTS = reduction_eager reduction_p reduction_po reduction_pos
check_PROGRAMS = $(TESTS)

ARGS = 15 100 100

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

noinst_HEADERS = reduction.h
 
all: $(TESTS)

testreduction: runreduction

runporting: $(TESTS)
	./reduction_opt1 $(ARGS)
	
reduction_eager_SOURCES = reduction.cpp

reduction_p_SOURCES = reduction.cpp
reduction_p_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND 

reduction_po_SOURCES = reduction.cpp
reduction_po_CPPFLAGS =   $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY

reduction_pos_SOURCES = reduction.cpp
reduction_pos_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE



