AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testcholesky runcholesky

TESTS = cholesky
check_PROGRAMS = $(TESTS)

ARGS = 1000 50 -i input/1000.in -o output/1000.out

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

cholesky_SOURCES = cholesky.cpp cholesky_cnc.cpp
noinst_HEADERS = cholesky.h cholesky_types.h

all: $(TESTS)

testblacksholes: runcholesky
	diff --ignore-space-change -q reference_output/1000.out output/1000.out

runcholesky: $(TESTS)
	./cholesky $(ARGS)

EXTRA_DIST = Readme.txt cholesky.cnc optionData.txt
