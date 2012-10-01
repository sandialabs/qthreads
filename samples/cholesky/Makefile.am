AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

OUTPUT=output/1000.out
INPUT=input/1000.in
REFERENCE_OUTPUT=reference_output/1000.out

.PHONY: testcholesky runcholesky

TESTS = cholesky_flexible.sh \
		cholesky_strict_simple.sh \
		cholesky_eager.sh \
		cholesky_strict.sh

output:
	$(MKDIR_P) output
	
check_PROGRAMS = cholesky_flexible \
				 cholesky_strict_simple \
				 cholesky_eager \
				 cholesky_strict

ARGS = $(top_srcdir)/samples/cholesky/$(INPUT) $(top_builddir)/samples/cholesky/$(OUTPUT) $(top_srcdir)/samples/cholesky/$(REFERENCE_OUTPUT)

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

noinst_HEADERS = cholesky.h cholesky_types.h

all: $(INPUT_FILES) $(TESTS) output

testcholesky: runcholesky
	diff --ignore-space-change -q $(REFERENCE_OUTPUT) $(OUTPUT)

runcholesky: $(INPUT_FILES) $(TESTS)
	./cholesky_flexible $(ARGS)

EXTRA_DIST = cholesky.cnc $(INPUT) $(REFERENCE_OUTPUT) $(TESTS)

cholesky_flexible.sh: cholesky_flexible

cholesky_strict_simple.sh: cholesky_strict_simple

cholesky_eager.sh: cholesky_eager

cholesky_strict.sh: cholesky_strict

cholesky_flexible_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_flexible_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

cholesky_strict_simple_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_strict_simple_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

cholesky_eager_SOURCES = cholesky.cpp cholesky_cnc.cpp


cholesky_strict_SOURCES = cholesky.cpp cholesky_cnc.cpp
cholesky_strict_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY

clean-local:
	$(RM) -rf $(top_builddir)/samples/cholesky/output
