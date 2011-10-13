#ifdef HAVE_CONFIG_H
# include "config.h"                   /* for _GNU_SOURCE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static aligned_t threads  = 0;
static aligned_t numincrs = 1024;
static size_t   *sleeplen;
static qtimer_t timer;
static size_t numiters = 10;

static void sumsleep(const size_t startat,
                const size_t stopat,
                void        *arg_)
{
    qthread_incr(&threads, stopat - startat);
    for (size_t i = startat; i < stopat; ++i) {
        usleep(sleeplen[i]);
    }
}

static void sum(const size_t startat,
                const size_t stopat,
                void        *arg_)
{
    qthread_incr(&threads, stopat - startat);
}

static void run_iterations(void (*loop)(const size_t a, const size_t b, const qt_loop_f f, void * c), qt_loop_f func, double overhead, const char *type, const char* name)
{
    double total    = 0;

    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        loop(0, numincrs, func, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("%-21s %-7s %8lu %f\n", type, name, (unsigned long)numincrs,
           (total / numiters) - overhead);
}

int main(int   argc,
         char *argv[])
{
    double overhead = 0;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(numincrs, "NUM_INCRS");
    NUMARG(numiters, "NUM_ITERS");
    printf("%i shepherds\n", qthread_num_shepherds());
    printf("%i threads\n", qthread_num_workers());
    timer = qtimer_create();

    sleeplen = malloc(sizeof(size_t) * numincrs);
    for (int i = 0; i < numincrs; ++i) {
        sleeplen[i] = qtimer_fastrand() % 1000;
        overhead   += sleeplen[i] / 1.0e6;
    }
    overhead /= qthread_num_workers();

    printf("Pure Increment\n");
    printf("%-21s %-7s %8s time\n", "version", "sync", "iters");

    qt_loop(0, numincrs, sum, NULL);
    run_iterations(qt_loop,         sum,0, "thread per iteration", "syncvar");
    run_iterations(qt_loop_aligned, sum,0, "thread per iteration", "aligned");
    run_iterations(qt_loop_sinc,    sum,0, "thread per iteration", "sinc");
    run_iterations(qt_loop_balance, sum,0, "balanced",             "syncvar");
    run_iterations(qt_loop_balance_aligned, sum,0, "balanced", "aligned");
    run_iterations(qt_loop_balance_sinc, sum,0, "balanced", "sinc");

    printf("\n");
    printf("Increment with Sleep (%f secs overhead)\n", overhead);
    printf("%-21s %-7s %8s time\n", "version", "sync", "iters");

    run_iterations(qt_loop, sumsleep, overhead, "thread per iteration", "syncvar");
    run_iterations(qt_loop_aligned, sumsleep, overhead, "thread per iteration", "aligned");
    run_iterations(qt_loop_sinc, sumsleep, overhead, "thread per iteration", "sinc");
    run_iterations(qt_loop_balance, sumsleep, overhead, "balanced", "syncvar");
    run_iterations(qt_loop_balance_aligned, sumsleep, overhead, "balanced", "aligned");
    run_iterations(qt_loop_balance_sinc, sumsleep, overhead, "balanced", "sinc");

    qtimer_destroy(timer);
    return 0;
}

/* vim:set expandtab */
