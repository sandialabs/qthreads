lib_LTLIBRARIES = libqthread.la
pkginclude_HEADERS = qthread/qthread.h \
					 qthread/qalloc.h \
					 qthread/qthread-int.h \
					 qthread/qutil.h \
					 qthread/qloop.h \
					 qthread/futurelib.h \
					 qthread/loop_templates.hpp \
					 qthread/loop_iter.hpp \
					 qthread/loop.hpp

libqthread_la_SOURCES = qthread.c qalloc.c qutil.c futurelib.c qloop.c
libqthread_la_LDFLAGS = -version-info 8:0:0 -no-undefined
EXTRA_libqthread_la_SOURCES = context.c asm.S
libqthread_la_LIBADD = @QTHREAD_EXTRA_OBJS@
libqthread_la_DEPENDENCIES = @QTHREAD_EXTRA_OBJS@

man_MANS = man3/qthread_empty.3 \
		   man3/qthread_feb_status.3 \
		   man3/qthread_fill.3 \
		   man3/qthread_finalize.3 \
		   man3/qthread_fork.3 \
		   man3/qthread_fork_to.3 \
		   man3/qthread_id.3 \
		   man3/qthread_init.3 \
		   man3/qthread_lock.3 \
		   man3/qthread_prepare.3 \
		   man3/qthread_prepare_for.3 \
		   man3/qthread_retloc.3 \
		   man3/qthread_readFE.3 \
		   man3/qthread_readFF.3 \
		   man3/qthread_schedule.3 \
		   man3/qthread_schedule_on.3 \
		   man3/qthread_self.3 \
		   man3/qthread_shep.3 \
		   man3/qthread_stackleft.3 \
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
		   man3/qalloc_statmalloc.3 \
		   man3/qutil_double_sum.3 \
		   man3/qutil_double_mult.3 \
		   man3/qutil_double_max.3 \
		   man3/qutil_double_min.3 \
		   man3/qutil_uint_sum.3 \
		   man3/qutil_uint_mult.3 \
		   man3/qutil_uint_max.3 \
		   man3/qutil_uint_min.3 \
		   man3/qutil_int_sum.3 \
		   man3/qutil_int_mult.3 \
		   man3/qutil_int_max.3 \
		   man3/qutil_int_min.3 \
		   man3/qutil_qsort.3 \
		   man3/qutil_mergesort.3 \
		   man3/qt_double_max.3 \
		   man3/qt_double_min.3 \
		   man3/qt_double_prod.3 \
		   man3/qt_double_sum.3 \
		   man3/qt_int_max.3 \
		   man3/qt_int_min.3 \
		   man3/qt_int_prod.3 \
		   man3/qt_int_sum.3 \
		   man3/qt_uint_max.3 \
		   man3/qt_uint_min.3 \
		   man3/qt_uint_prod.3 \
		   man3/qt_uint_sum.3 \
		   man3/qt_loop.3 \
		   man3/qt_loop_future.3 \
		   man3/qt_loop_balance.3 \
		   man3/qt_loop_balance_future.3 \
		   man3/qt_loopaccum_balance.3 \
		   man3/qt_loopaccum_balance_future.3 \
		   man3/future_acquire.3 \
		   man3/future_fork.3 \
		   man3/future_exit.3 \
		   man3/future_init.3 \
		   man3/future_join_all.3 \
		   man3/future_yield.3

TESTS = test1 test2 test3 testq testloop testincr testqloop
check_PROGRAMS = test1 test2 test3 testq testloop testincr testqloop
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
testqloop_SOURCES = testqloop.c
testqloop_LDADD = -lqthread
testincr_SOURCES = testincr.c
testincr_LDADD = -lqthread

#test2: test2.c
#	$(CC) -I. $(CFLAGS) -L.libs/libqthread.a -o $< $^

distclean-local:
	find . -name .DS_Store -exec $(RM) {} \;
	find . -name .cvsignore -exec $(RM) {} \;
	$(RM) .gdb_history
	$(RM) -rf autom4te.cache
	$(RM) *~
