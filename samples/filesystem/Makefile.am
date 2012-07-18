AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = filesystem
check_PROGRAMS = $(TESTS)

ARGS = 256
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testfilesystem runfilesystem

filesystem_SOURCES = filesystem.cpp
noinst_HEADERS = filesystem.h filesystem_types.h

all: $(TESTS)

testfilesystem: $(TESTS)
	./filesystem $(ARGS)

runfilesystem: testfilesystem

EXTRA_DIST = Readme.txt filesystem.cnc
