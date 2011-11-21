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
static size_t    numiters = 10;
static qtimer_t  timer;

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

static void sum(void *a_,
                void *b_)
{
    *(size_t*)a_ += *(size_t*)b_;
    //qthread_incr((size_t *)a_, *(size_t *)b_);
}

#if 0
static void sum_sinc(void *a_,
                     void *b_)
{
    *(size_t *)a_ += *(size_t *)b_;
}

#endif

static void run_iterations(void                                                   (*loop)(const size_t a,
                                                                 const size_t     b,
                                                                 const size_t     sz,
                                                                 void            *out,
                                                                 const qt_loopr_f f,
                                                                 void            *c,
                                                                 const qt_accum_f acc),
                           qt_loopr_f                                             func,
                           qt_accum_f                                             acc,
                           double                                                 overhead,
                           const char                                            *type,
                           const char                                            *name)
{
    double total  = 0;
    int    passed = 0;
    int    i;

    // to warm the caches
    threads=0;
    loop(0, numincrs, sizeof(aligned_t), &threads, func, NULL, acc);
    // now, to time it
    for (i = 0; i < numiters; ++i) {
        threads = 0;
        qtimer_start(timer);
        loop(0, numincrs, sizeof(aligned_t), &threads, func, NULL, acc);
        qtimer_stop(timer);
        passed = threads == numincrs;
        total += qtimer_secs(timer);
        if (!passed) {
            break;
        }
    }
    printf("%-21s %-9s %5lu %8lu %8f %s\n", type, name, (unsigned long)numiters,
           (unsigned long)numincrs,
           (total / i) - overhead,
           passed ? "PASSED" : "FAILED");
    if (!passed) {
	printf("threads = %lu, numincrs = %lu\n", (unsigned long)threads, (unsigned long)numincrs);
    }
}

int main(int   argc,
         char *argv[])
{
    double overhead = 0;

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

    timer = qtimer_create();

    printf("%-21s %-9s %5s %8s %s\n", "version", "sync", "iters", "incrs", "time");
    run_iterations(qt_loopaccum_balance_syncvar,      count, sum, overhead, "balanced", "syncvar");
    run_iterations(qt_loopaccum_balance_sinc, count, sum, overhead, "balanced", "sinc");
    run_iterations(qt_loopaccum_balance_dc, count, sum, overhead, "balanced", "donecount");

    return 0;
}

/* vim:set expandtab */
