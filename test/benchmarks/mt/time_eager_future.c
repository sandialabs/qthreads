#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif

#include <assert.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static aligned_t global_scratch = 0;
static uint64_t  num_iterations = 0;

static aligned_t null_task(void *args_)
{
    aligned_t d = 0;

    for (uint64_t i = 0; i < num_iterations; i++) d += (2.0 * i + 1);
    return d;
}

int main(int   argc,
         char *argv[])
{
    uint64_t count = 0;

    qtimer_t timer;
    double   total_time = 0.0;

    CHECK_VERBOSE();

    NUMARG(num_iterations, "MT_NUM_ITERATIONS");
    NUMARG(count, "MT_COUNT");
    assert(0 != count);

    assert(qthread_initialize() == 0);

    syncvar_t rets[count];
    for (uint64_t i = 0; i < count; i++) rets[i] = SYNCVAR_EMPTY_INITIALIZER;

    timer = qtimer_create();
    qtimer_start(timer);

    for (uint64_t i = 0; i < count; i++) qthread_fork_syncvar(null_task, NULL, &rets[i]);

    aligned_t tmp;
    for (uint64_t i = 0; i < count; i++) {
        qthread_syncvar_readFF(&tmp, &rets[i]);
        global_scratch += tmp;
    }

    qtimer_stop(timer);

    total_time = qtimer_secs(timer);

    qtimer_destroy(timer);

    printf("%lu %lu %lu %f\n",
           (unsigned long)qthread_num_workers(),
           (unsigned long)count,
           (unsigned long)num_iterations,
           total_time);

    return 0;
}

/* vim:set expandtab */
