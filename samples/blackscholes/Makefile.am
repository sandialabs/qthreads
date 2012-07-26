AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = blackscholes_opt1 blackscholes_opt2 blackscholes_opt3
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

blackscholes_opt1_SOURCES = blackscholes.cpp
blackscholes_opt1_CPPFLAGS = -DCNC_PRECOND

blackscholes_opt2_SOURCES = blackscholes.cpp
blackscholes_opt3_CPPFLAGS = -DCNC_PRECOND -DCNC_PRECOND_ONLY

blackscholes_opt3_SOURCES = blackscholes.cpp
