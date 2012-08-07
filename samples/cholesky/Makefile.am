AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x
OUTPUT=output/1000.out
.PHONY: testcholesky runcholesky

TESTS = cholesky_opt1 cholesky_opt2 cholesky_opt3 cholesky_opt4
check_PROGRAMS = $(TESTS)

ARGS = 1000 50 -i $(top_srcdir)/samples/cholesky/input/1000.in -o $(top_srcdir)/samples/cholesky/output/1000.out

TESTS_ENVIRONMENT = rm -rf $(top_srcdir)/samples/cholesky/output/1000.in && echo $(ARGS) | xargs

noinst_HEADERS = cholesky.h cholesky_types.h

all: $(TESTS)

testcholesky: runcholesky
	diff --ignore-space-change -q reference_output/1000.out $(OUTPUT)

runcholesky: $(TESTS)
	- rm $(OUTPUT)
	./cholesky_opt1 $(ARGS)

EXTRA_DIST = cholesky.cnc

cholesky_opt1_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt1_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

cholesky_opt2_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt2_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

cholesky_opt3_SOURCES = cholesky.cpp cholesky_cnc.cpp


cholesky_opt4_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt4_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY

