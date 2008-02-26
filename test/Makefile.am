TESTS = test1 test2 test3 testq testloop testincr testqloop testid
check_PROGRAMS = $(TESTS)

INCLUDES = -I$(top_srcdir)/include
QTHREADLIB = $(top_builddir)/src/libqthread.la

test1_SOURCES = test1.c
test1_LDADD = $(QTHREADLIB)

test2_SOURCES = test2.c
test2_LDADD = $(QTHREADLIB)

test3_SOURCES = test3.c
test3_LDADD = $(QTHREADLIB)

testq_SOURCES = testq.c
testq_LDADD = $(QTHREADLIB)

testloop_SOURCES = testloop.cpp
testloop_LDADD = $(QTHREADLIB)

testqloop_SOURCES = testqloop.c
testqloop_LDADD = $(QTHREADLIB)

testincr_SOURCES = testincr.c
testincr_LDADD = $(QTHREADLIB)

testid_SOURCES = test_id.c
testid_LDADD = $(QTHREADLIB)
