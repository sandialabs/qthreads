AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = filesystem_flexible filesystem_strict_simple filesystem_eager filesystem_strict
check_PROGRAMS = $(TESTS)

ARGS = 256
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testfilesystem runfilesystem

noinst_HEADERS = filesystem.h filesystem_types.h

all: $(TESTS)

testfilesystem: $(TESTS)
	./filesystem $(ARGS)

runfilesystem: testfilesystem

EXTRA_DIST = Readme.txt filesystem.cnc

filesystem_flexible_SOURCES = filesystem.cpp
filesystem_flexible_CPPFLAGS =  $(AM_CPPFLAGS)  -DCNC_PRECOND

filesystem_strict_simple_SOURCES = filesystem_strict.cpp
filesystem_strict_simple_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

filesystem_eager_SOURCES = filesystem.cpp

filesystem_strict_SOURCES = filesystem_strict.cpp
filesystem_strict_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
