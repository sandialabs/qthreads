AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = blackscholes_flexible blackscholes_strict_simple blackscholes_eager blackscholes_strict
check_PROGRAMS = $(TESTS)

ARGS = 100000 100 4
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testblackscholes runblackscholes

noinst_HEADERS = blackscholes.h

all: $(TESTS)

testblackscholes: $(TESTS)
	./blackscholes $(ARGS)

runblackscholes: testblackscholes

EXTRA_DIST = Readme.txt blackscholes.cnc optionData.txt

blackscholes_flexible_SOURCES = blackscholes.cpp
blackscholes_flexible_CPPFLAGS = $(AM_CPPFLAGS) -DCNC_PRECOND

blackscholes_strict_simple_SOURCES = blackscholes.cpp
blackscholes_strict_simple_CPPFLAGS = $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

blackscholes_eager_SOURCES = blackscholes.cpp

blackscholes_strict_SOURCES = blackscholes.cpp
blackscholes_strict_CPPFLAGS = $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
