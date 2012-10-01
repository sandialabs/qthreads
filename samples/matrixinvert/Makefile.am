AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testmatrixinvert runmatrixinvert

TESTS = matrixinvert_flexible matrixinvert_strict_simple matrixinvert_eager matrixinvert_strict
check_PROGRAMS = $(TESTS)

ARGS = 1024 1

TESTS_ENVIRONMENT = echo $(ARGS) | env QT_STACK_SIZE=200000 xargs

noinst_HEADERS = tile.h matrixinvert.h

all: $(TESTS)

testmatrixinvert: runmatrixinvert

runmatrixinvert: $(TESTS)
	env QT_STACK_SIZE=200000 ./matrixinvert $(ARGS)

EXTRA_DIST = matrixinvert.cnc

matrixinvert_flexible_SOURCES = matrixinvert.cpp
matrixinvert_flexible_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND

matrixinvert_strict_simple_SOURCES = matrixinvert.cpp
matrixinvert_strict_simple_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

matrixinvert_eager_SOURCES = matrixinvert.cpp

matrixinvert_strict_SOURCES = matrixinvert.cpp
matrixinvert_strict_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
