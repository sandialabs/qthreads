AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

SHELL=/bin/bash 
OUTPUT=output/1000.out
INPUT=input/1000.in
REFERENCE_OUTPUT=reference_output/1000.out

.PHONY: testcholesky runcholesky precheck

precheck:
	pwd
	if [ -e $(top_srcdir)/samples/cholesky/$(INPUT) ]; \
	then \
		echo "Hooray... I have a  file!"; \
	else \
		cd $(top_srcdir)/samples/cholesky/input; ./unpack.sh;\
	fi
	- rm $(OUTPUT)



TESTS = cholesky_opt1 cholesky_opt2 cholesky_opt3 cholesky_opt4


	
check_PROGRAMS = $(TESTS)

ARGS = 1000 50 -i $(top_srcdir)/samples/cholesky/$(INPUT) -o $(top_srcdir)/samples/cholesky/$(OUTPUT)

TESTS_ENVIRONMENT = rm -rf $(top_srcdir)/samples/cholesky/$(OUTPUT) ; echo $(ARGS) | xargs

noinst_HEADERS = cholesky.h cholesky_types.h

all: precheck $(TESTS)

testcholesky: runcholesky
	diff --ignore-space-change -q $(REFERENCE_OUTPUT) $(OUTPUT)

runcholesky: precheck $(TESTS)
	
	./cholesky_opt1 $(ARGS)

EXTRA_DIST = cholesky.cnc input input/unpack.sh input/cholesky_inputs.tar.bz2 output

cholesky_opt1_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt1_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

cholesky_opt2_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt2_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

cholesky_opt3_SOURCES = cholesky.cpp cholesky_cnc.cpp


cholesky_opt4_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt4_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY

