headers = qthread.h \
		  qalloc.h \
		  qutil.h \
		  qloop.h \
		  futurelib.h \
		  loop_templates.hpp \
		  loop_iter.hpp \
		  loop.hpp

qthreaddir = $(includedir)/qthread
dist_qthread_HEADERS = $(headers)
nodist_qthread_HEADERS = \
	common.h \
	qthread-int.h

distclean-local:
	$(RM) -f qthread-int.h

uninstall-local:
	test -d "$(qthreaddir)" && rmdir $(qthreaddir) || true
