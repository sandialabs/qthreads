lib_LTLIBRARIES = libqthread.la
pkginclude_HEADERS = qthread.h qalloc.h qthread-int.h futurelib.h

libqthread_la_SOURCES = qthread.c qalloc.c futurelib.c
libqthread_la_LDFLAGS = -version-info 4:0:0 -no-undefined
EXTRA_libqthread_la_SOURCES = context.c asm.S
libqthread_la_LIBADD = @QTHREAD_EXTRA_OBJS@
libqthread_la_DEPENDENCIES = @QTHREAD_EXTRA_OBJS@

man_MANS = man3/qthread_empty.3 \
		   man3/qthread_feb_status.3 \
		   man3/qthread_fill.3 \
		   man3/qthread_finalize.3 \
		   man3/qthread_fork.3 \
		   man3/qthread_fork_detach.3 \
		   man3/qthread_fork_to.3 \
		   man3/qthread_fork_to_detach.3 \
		   man3/qthread_id.3 \
		   man3/qthread_init.3 \
		   man3/qthread_join.3 \
		   man3/qthread_lock.3 \
		   man3/qthread_prepare.3 \
		   man3/qthread_prepare_detached.3 \
		   man3/qthread_prepare_for.3 \
		   man3/qthread_prepare_for_detached.3 \
		   man3/qthread_readFE.3 \
		   man3/qthread_readFF.3 \
		   man3/qthread_schedule.3 \
		   man3/qthread_schedule_on.3 \
		   man3/qthread_self.3 \
		   man3/qthread_shep.3 \
		   man3/qthread_unlock.3 \
		   man3/qthread_writeEF.3 \
		   man3/qthread_writeEF_const.3 \
		   man3/qthread_writeF.3 \
		   man3/qthread_writeF_const.3 \
		   man3/qthread_yield.3 \
		   man3/qalloc_checkpoint.3 \
		   man3/qalloc_cleanup.3 \
		   man3/qalloc_dynfree.3 \
		   man3/qalloc_dynmalloc.3 \
		   man3/qalloc_free.3 \
		   man3/qalloc_loadmap.3 \
		   man3/qalloc_makedynmap.3 \
		   man3/qalloc_makestatmap.3 \
		   man3/qalloc_malloc.3 \
		   man3/qalloc_statfree.3 \
		   man3/qalloc_statmalloc.3

TESTS = test1 test2 test3 testq testloop
check_PROGRAMS = test1 test2 test3 testq testloop
test1_SOURCES = test.c
test1_LDADD = -lqthread
test2_SOURCES = test2.c
test2_LDADD = -lqthread
test3_SOURCES = test3.c
test3_LDADD = -lqthread
testq_SOURCES = testq.c
testq_LDADD = -lqthread
testloop_SOURCES = testloop.cpp
testloop_LDADD = -lqthread

#test2: test2.c
#	$(CC) -I. $(CFLAGS) -L.libs/libqthread.a -o $< $^

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
