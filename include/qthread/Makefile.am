headers = qthread.h \
		  qalloc.h \
		  qthread-int.h \
		  common.h \
		  qutil.h \
		  qloop.h \
		  futurelib.h \
		  loop_templates.hpp \
		  loop_iter.hpp \
		  loop.hpp

qthreaddir = $(includedir)/qthread
qthread_HEADERS = $(headers)

uninstall-local:
	test -d "$(qthreaddir)" && rmdir $(qthreaddir) || true
