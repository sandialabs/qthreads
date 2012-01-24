#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif

#include <assert.h>
#include <stdio.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

//static aligned_t donecount = 0;

static double   global_scratch = 0;
static unsigned long num_iterations = 0;

static double delay(void)
{
    double d = 0;
    unsigned long i;

    for (i = 0; i < num_iterations; i++) d += 1 / (2.0 * i + 1);
    return d;
}

static aligned_t null_task(void *args_)
{
    global_scratch = delay();

    return 0;
}

int main(int   argc,
         char *argv[])
{
    unsigned long count = 0;
    unsigned long i;
    int par_fork = 0;

    qtimer_t timer;
    double   total_time = 0.0;

    CHECK_VERBOSE();

    NUMARG(num_iterations, "MT_NUM_ITERATIONS");
    NUMARG(count, "MT_COUNT");
    NUMARG(par_fork, "MT_PAR_FORK");
    assert(0 != count);

    timer = qtimer_create();
    qtimer_start(timer);

    if (par_fork) {
	_Cilk_for (i = 0; i < count; i++) {
	    _Cilk_spawn null_task(NULL);
	}
    } else {
	for (i = 0; i < count; i++) {
	    _Cilk_spawn null_task(NULL);
	}
    }
    _Cilk_sync;

    qtimer_stop(timer);

    total_time = qtimer_secs(timer);

    qtimer_destroy(timer);

    printf("%d %lu %lu %f\n",
           __cilkrts_get_nworkers(),
           (unsigned long)count,
           (unsigned long)num_iterations,
           total_time);

    return 0;
}

/* vim:set expandtab */
