#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif

#include <assert.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static void par_null_task(size_t start,
                          size_t stop,
                          void  *args_)
{}

static void par_null_task2(size_t start,
                          size_t stop,
                          void  *args_)
{
    for (size_t i=start; i<stop; ++i) {
    }
}

int main(int   argc,
         char *argv[])
{
    uint64_t count      = 1048576;
    uint64_t loop_style = 1;

    qtimer_t timer;
    double   total_time = 0.0;

    CHECK_VERBOSE();

    NUMARG(loop_style, "LOOP_STYLE");
    NUMARG(count, "MT_COUNT");
    assert(0 != count);

    assert(qthread_initialize() == 0);

    timer = qtimer_create();

    switch (loop_style) {
	case 1:
	    qtimer_start(timer);
	    qt_loop(0, count, par_null_task, NULL);
	    qtimer_stop(timer);
	    break;
	case 2:
	    qtimer_start(timer);
	    qt_loop_parallel(0, count, par_null_task2, NULL);
	    qtimer_stop(timer);
	    break;
	case 3:
	    qtimer_start(timer);
	    qqloop_handle_t *l = qt_loop_queue_create(CHUNK, 0, count, 1, par_null_task2, NULL);
	    qt_loop_queue_run(l);
	    qtimer_stop(timer);
	    break;
	case 4:
	    qtimer_start(timer);
	    qqloop_handle_t *l = qt_loop_queue_create(GUIDED, 0, count, 1, par_null_task2, NULL);
	    qt_loop_queue_run(l);
	    qtimer_stop(timer);
	    break;
	case 5:
	    qtimer_start(timer);
	    qqloop_handle_t *l = qt_loop_queue_create(FACTORED, 0, count, 1, par_null_task2, NULL);
	    qt_loop_queue_run(l);
	    qtimer_stop(timer);
	    break;
	case 6:
	    qtimer_start(timer);
	    qqloop_handle_t *l = qt_loop_queue_create(TIMED, 0, count, 1, par_null_task2, NULL);
	    qt_loop_queue_run(l);
	    qtimer_stop(timer);
	    break;
    }

    total_time = qtimer_secs(timer);

    qtimer_destroy(timer);

    printf("%lu %lu %f\n",
           (unsigned long)qthread_num_workers(),
           (unsigned long)count,
           total_time);

    return 0;
}

/* vim:set expandtab */
