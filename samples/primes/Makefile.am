AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = primes_flexible primes_strict_simple primes_eager primes_strict
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

primes_flexible_SOURCES = primes.cpp
primes_flexible_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

primes_strict_simple_SOURCES = primes.cpp
primes_strict_simple_CPPFLAGS =  $(AM_CPPFLAGS)  -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

primes_eager_SOURCES = primes.cpp

primes_strict_SOURCES = primes.cpp
primes_strict_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
