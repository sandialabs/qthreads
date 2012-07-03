AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = primes
check_PROGRAMS = $(TESTS)

ARGS = 1000 167
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

primes_SOURCES = primes.cpp

.PHONY: testprimes runprimes

all: $(TESTS)

testprimes: $(TESTS)
	./primes $(ARGS)

runprimes: $(TESTS)
	./primes 1000

EXTRA_DIST = primes.cnc
