#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <float.h> /* for DBL_MAX, per C89 */
#include <signal.h>
#include <qthread/qthread.h>
#include <qthread/barrier.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

aligned_t     initme_idx = 1;
aligned_t    *initme     = NULL;
qt_barrier_t *wait_on_me;
int stack_size;
size_t       threads = 2;
aligned_t    *rets;

int main(int   argc,
         char *argv[])
{
    int i;
    qtimer_t     t;
    unsigned int iter, iterations = 1;
    double       total_time = 0.0;
    double       max_time   = 0.0;
    double       min_time   = DBL_MAX;
    struct sigaction sa;
    /*
    assert(qthread_initialize() == 0);

    CHECK_VERBOSE();
    NUMARG(threads, "THREADS");
    NUMARG(iterations, "ITERATIONS");
    if (!getenv("QT_STACK_SIZE") && !getenv("QTHREAD_STACK_SIZE"))
      setenv("QT_STACK_SIZE", "4096", 0);

    stack_size = atoi(getenv("QT_STACK_SIZE"));
    rets = (aligned_t *)malloc(threads * sizeof(aligned_t));
    assert(rets);
    */
    sleep(4);
    exit(0);
}

/* vim:set expandtab */
