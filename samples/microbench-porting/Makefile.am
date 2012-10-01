AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

.PHONY: testporting runporting

TESTS = porting_strict porting_flexible
check_PROGRAMS = $(TESTS)

ARGS = 3 50000 20

TESTS_ENVIRONMENT = echo $(ARGS) | xargs

noinst_HEADERS = porting.h

all: $(TESTS)

testporting: runporting

runporting: $(TESTS)
	./porting_strict $(ARGS)

EXTRA_DIST = porting.cnc

porting_strict_SOURCES = porting_strict.cpp
porting_strict_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND -DCNC_PRECOND_ONLY -DCNC_PRECOND_SIMPLE

porting_flexible_SOURCES = porting.cpp
porting_flexible_CPPFLAGS =  $(AM_CPPFLAGS) -DCNC_PRECOND
