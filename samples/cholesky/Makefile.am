AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

OUTPUT=output/1000.out
INPUT=input/1000.in
REFERENCE_OUTPUT=reference_output/1000.out

.PHONY: testcholesky runcholesky

TESTS = cholesky_opt1.sh \
		cholesky_opt2.sh \
		cholesky_opt3.sh \
		cholesky_opt4.sh

output:
	$(MKDIR_P) output
	
check_PROGRAMS = cholesky_opt1 \
				 cholesky_opt2 \
				 cholesky_opt3 \
				 cholesky_opt4

ARGS = $(top_srcdir)/samples/cholesky/$(INPUT) $(top_builddir)/samples/cholesky/$(OUTPUT) $(top_srcdir)/samples/cholesky/$(REFERENCE_OUTPUT)

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

noinst_HEADERS = cholesky.h cholesky_types.h

all: $(INPUT_FILES) $(TESTS) output

testcholesky: runcholesky
	diff --ignore-space-change -q $(REFERENCE_OUTPUT) $(OUTPUT)

runcholesky: $(INPUT_FILES) $(TESTS)
	./cholesky_opt1 $(ARGS)

EXTRA_DIST = cholesky.cnc $(INPUT) $(REFERENCE_OUTPUT) $(TESTS)

cholesky_opt1.sh: cholesky_opt1

cholesky_opt2.sh: cholesky_opt2

cholesky_opt3.sh: cholesky_opt3

cholesky_opt4.sh: cholesky_opt4

cholesky_opt1_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt1_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

cholesky_opt2_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt2_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

cholesky_opt3_SOURCES = cholesky.cpp cholesky_cnc.cpp


cholesky_opt4_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_opt4_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY

clean-local:
	$(RM) -rf $(top_builddir)/samples/cholesky/output
