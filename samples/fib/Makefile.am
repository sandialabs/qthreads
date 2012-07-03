AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = fib
check_PROGRAMS = $(TESTS)

ARGS = 512 7623193360756408773
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

fib_SOURCES = fib.cpp
noinst_HEADERS = fib.h

.PHONY: testfib runfib

all: $(TESTS)

testfib: $(TESTS)
	./fib $(ARGS)

runfib: $(TESTS)
	./fib 512
