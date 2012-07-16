AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_CXXFLAGS = -std=gnu++0x

TESTS = routing
check_PROGRAMS = $(TESTS)

ARGS = 100 100
TESTS_ENVIRONMENT = echo $(ARGS) | xargs

.PHONY: testrouting runrouting

routing_SOURCES = routing.cpp
noinst_HEADERS = routing.h routing_types.h

all: $(TESTS)

testrouting: $(TESTS)
	./routing $(ARGS)

runrouting: testrouting

EXTRA_DIST = Readme.txt routing.cnc
