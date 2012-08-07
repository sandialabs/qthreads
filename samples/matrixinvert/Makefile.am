AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testmatrixinvert runmatrixinvert

TESTS = matrixinvert_opt1 matrixinvert_opt2 matrixinvert_opt3 matrixinvert_opt4
check_PROGRAMS = $(TESTS)

ARGS = 1024 1

TESTS_ENVIRONMENT = echo $(ARGS) | env QT_STACK_SIZE=200000 xargs

noinst_HEADERS = tile.h matrixinvert.h

all: $(TESTS)

testmatrixinvert: runmatrixinvert

runmatrixinvert: $(TESTS)
	env QT_STACK_SIZE=200000 ./matrixinvert $(ARGS)

EXTRA_DIST = matrixinvert.cnc

matrixinvert_opt1_SOURCES = matrixinvert.cpp
matrixinvert_opt1_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

matrixinvert_opt2_SOURCES = matrixinvert.cpp
matrixinvert_opt2_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

matrixinvert_opt3_SOURCES = matrixinvert.cpp

matrixinvert_opt4_SOURCES = matrixinvert.cpp
matrixinvert_opt4_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
