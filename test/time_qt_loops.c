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

int main(int   argc,
         char *argv[])
{
    double total    = 0;
    double overhead = 0;
    size_t numiters = 10;
    qtimer_t timer = qtimer_create();

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(numincrs, "NUM_INCRS");
    NUMARG(numiters, "NUM_ITERS");
    printf("%i shepherds\n", qthread_num_shepherds());
    printf("%i threads\n", qthread_num_workers());

    sleeplen = malloc(sizeof(size_t) * numincrs);
    for (int i = 0; i < numincrs; ++i) {
        sleeplen[i] = qtimer_fastrand() % 1000;
        overhead   += sleeplen[i] / 1.0e6;
    }
    overhead /= qthread_num_workers();

    printf("Pure Increment\n");
    printf("version              sync        iters    time\n");

    qt_loop(0, numincrs, sum, NULL);
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop(0, numincrs, sum, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration syncvar %8lu %f\n", (unsigned long)numincrs,
           (total / numiters));

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_sinc(0, numincrs, sum, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration sinc    %8lu %f\n", (unsigned long)numincrs,
           (total / numiters));

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_balance(0, numincrs, sum, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("balanced             syncvar %8lu %f\n", (unsigned long)numincrs,
           (total / numiters));

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_balance_sinc(0, numincrs, sum, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("balanced             sinc    %8lu %f\n", (unsigned long)numincrs,
           (total / numiters));

    printf("\n");
    printf("Increment with Sleep (%f secs overhead)\n", overhead);
    printf("version              sync        iters    time\n");

    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop(0, numincrs, sumsleep, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration syncvar %8lu %f\n", (unsigned long)numincrs,
           (total / numiters) - overhead);

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_sinc(0, numincrs, sumsleep, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration sinc    %8lu %f\n", (unsigned long)numincrs,
           (total / numiters) - overhead);

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_balance(0, numincrs, sumsleep, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("balanced             syncvar %8lu %f\n", (unsigned long)numincrs,
           (total / numiters) - overhead);

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loop_balance_sinc(0, numincrs, sumsleep, NULL);
        assert(threads == numincrs);
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("balanced             sinc    %8lu %f\n", (unsigned long)numincrs,
           (total / numiters) - overhead);

    qtimer_destroy(timer);
    return 0;
}

/* vim:set expandtab */
