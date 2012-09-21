AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = filesystem_opt1 filesystem_opt2 filesystem_opt3 filesystem_opt4
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

filesystem_opt1_SOURCES = filesystem.cpp
filesystem_opt1_CPPFLAGS =  $(AM_CPPFLAGS)  -DCNC_PRECOND

filesystem_opt2_SOURCES = filesystem_strict.cpp
filesystem_opt2_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

filesystem_opt3_SOURCES = filesystem.cpp

filesystem_opt4_SOURCES = filesystem_strict.cpp
filesystem_opt4_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY
