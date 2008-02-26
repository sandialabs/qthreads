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

# These are generated files, and so should be removed by distclean
distclean-local:
	$(RM) common.h qthread-int.h
