#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif

#include <assert.h>
#include <stdio.h>
#include <omp.h>

#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static aligned_t donecount = 0;

static double   global_scratch = 0;
static uint64_t num_iterations = 0;

static double delay(void)
{
    double d = 0;

    for (uint64_t i = 0; i < num_iterations; i++) d += 1 / (2.0 * i + 1);
    return d;
}

static aligned_t null_task(void *args_)
{
    global_scratch = delay();

    return qthread_incr(&donecount, 1);
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

#pragma omp parallel
#pragma omp single
    {
        timer = qtimer_create();
        qtimer_start(timer);

        for (uint64_t i = 0; i < count; i++) {
#pragma omp task untied
            null_task(NULL);
        }

        /*while (donecount != count) {
         *  __asm__ __volatile__ ("":::"memory");
         * #pragma omp taskyield
         * }*/
#pragma omp taskwait

        qtimer_stop(timer);

        total_time = qtimer_secs(timer);

        qtimer_destroy(timer);

        printf("%lu %lu %lu %f %f\n",
               (unsigned long)omp_get_num_threads(),
               (unsigned long)count,
               (unsigned long)num_iterations,
               total_time,
               total_time / count);
    }

    return 0;
}

/* vim:set expandtab */
