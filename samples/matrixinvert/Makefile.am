AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testmatrixinvert runmatrixinvert

TESTS = matrixinvert
check_PROGRAMS = $(TESTS)

ARGS = 1024

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

matrixinvert_SOURCES = matrixinvert.cpp
noinst_HEADERS = tile.h matrixinvert.h

all: $(TESTS)

testmatrixinvert: runmatrixinvert

runmatrixinvert: $(TESTS)
	env QT_STACK_SIZE=200000 ./matrixinvert $(ARGS)

EXTRA_DIST = matrixinvert.cnc
