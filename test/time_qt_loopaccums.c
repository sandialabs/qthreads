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

static void count(const size_t startat,
                  const size_t stopat,
                  void        *arg_,
		  void        *ret_)
{
    for (size_t i = startat; i < stopat; ++i) {
        usleep(sleeplen[i]);
    }
    *(size_t *)ret_ = stopat - startat;
}

static void sum(void *a_, void *b_)
{
    qthread_incr((size_t *)a_, *(size_t *)b_);
}

static void sum_sinc(void *a_, void *b_)
{
    *(size_t *)a_ += *(size_t *)b_;
}

int main(int   argc,
         char *argv[])
{
    double total    = 0;
    double overhead = 0;
    size_t numiters = 10;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(numincrs, "NUM_INCRS");
    NUMARG(numiters, "NUM_ITERS");
    iprintf("%i shepherds\n", qthread_num_shepherds());
    iprintf("%i threads\n", qthread_num_workers());

    sleeplen = malloc(sizeof(size_t) * numincrs);
    for (int i = 0; i < numincrs; ++i) {
        sleeplen[i] = qtimer_fastrand() % 1000;
        overhead   += sleeplen[i] / 1.0e6;
    }
    overhead /= qthread_num_workers();

    iprintf("version              sync    iters    time\n");
    qtimer_t timer = qtimer_create();

    int passed = 0;

    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loopaccum_balance(0, numincrs, 
                             sizeof(aligned_t),
                             &threads,
                             count, NULL,
                             sum);
        passed = threads == numincrs;
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration syncvar %8lu %f %s\n", (unsigned long)numincrs,
           (total / numiters) - overhead, passed ? "PASSED" : "FAILED");

    total = 0;
    for (int i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        qt_loopaccum_balance_sinc(0, numincrs, 
                                  sizeof(aligned_t),
                                  &threads,
                                  count, NULL,
                                  sum);
        passed = threads == numincrs;
        qtimer_stop(timer);
        total += qtimer_secs(timer);
    }
    printf("thread per iteration sinc    %8lu %f %s\n", (unsigned long)numincrs,
           (total / numiters) - overhead, passed ? "PASSED" : "FAILED");

    return 0;
}

/* vim:set expandtab */
