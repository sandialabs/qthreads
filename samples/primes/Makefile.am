AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = primes_opt1 primes_opt2 primes_opt3
check_PROGRAMS = $(TESTS)

ARGS = 1000 167
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testprimes runprimes

all: $(TESTS)

testprimes: $(TESTS)
	./primes $(ARGS)

runprimes: $(TESTS)
	./primes 1000

EXTRA_DIST = primes.cnc

primes_opt1_SOURCES = primes.cpp
primes_opt1_CPPFLAGS = -DCNC_PRECOND

primes_opt2_SOURCES = primes.cpp
primes_opt2_CPPFLAGS = -DCNC_PRECOND -DCNC_PRECOND_ONLY

primes_opt3_SOURCES = primes.cpp
