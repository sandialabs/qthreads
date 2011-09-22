#ifdef HAVE_CONFIG_H
# include "config.h"                   /* for _GNU_SOURCE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static aligned_t threads = 0;
static aligned_t numincrs = 1024;

static void sum(const size_t startat,
                const size_t stopat,
                void        *arg_)
{
    qthread_incr(&threads, stopat - startat);
}

int main(int   argc,
         char *argv[])
{
    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(numincrs, "NUM_INCRS");
    iprintf("%i shepherds\n", qthread_num_shepherds());
    iprintf("%i threads\n", qthread_num_workers());

    iprintf("version    sync    iters    time\n");
    qtimer_t timer = qtimer_create();

    qtimer_start(timer);
    qt_loop(0, numincrs, sum, NULL);
    assert(threads == numincrs);
    qtimer_stop(timer);
    printf("unbalanced syncvar %8lu %f\n", (unsigned long)numincrs, 
        qtimer_secs(timer));

    threads = 0;
    qtimer_start(timer);
    qt_loop_sinc(0, numincrs, sum, NULL);
    assert(threads == numincrs);
    qtimer_stop(timer);
    printf("unbalanced sinc    %8lu %f\n", (unsigned long)numincrs, 
        qtimer_secs(timer));

    threads = 0;
    qtimer_start(timer);
    qt_loop_balance(0, numincrs, sum, NULL);
    assert(threads == numincrs);
    qtimer_stop(timer);
    printf("balanced   syncvar %8lu %f\n", (unsigned long)numincrs, 
        qtimer_secs(timer));

    threads = 0;
    qtimer_start(timer);
    qt_loop_balance_sinc(0, numincrs, sum, NULL);
    assert(threads == numincrs);
    qtimer_stop(timer);
    printf("unbalanced sinc    %8lu %f\n", (unsigned long)numincrs, 
        qtimer_secs(timer));

    return 0;
}

/* vim:set expandtab */
