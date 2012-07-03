AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = blackscholes
check_PROGRAMS = $(TESTS)

ARGS = 100000 100 4
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testblackscholes runblackscholes

blackscholes_SOURCES = blackscholes.cpp
noinst_HEADERS = blackscholes.h

all: $(TESTS)

testblackscholes: $(TESTS)
	./blackscholes $(ARGS)

runblackscholes: testblackscholes

EXTRA_DIST = Readme.txt blackscholes.cnc optionData.txt
