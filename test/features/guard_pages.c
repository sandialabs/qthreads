#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <float.h> /* for DBL_MAX, per C89 */
#include <qthread/qthread.h>
#include <qthread/barrier.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

aligned_t     initme_idx = 1;
aligned_t    *initme     = NULL;
qt_barrier_t *wait_on_me;

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    exit(0);
}

static aligned_t segfault_thread(void *arg)
{
    volatile char *eos; // end of stack
    eos = &eos + qt_stack_size;
    *eos = 1;
    // get the stack size
    // this might be slightly more complicated
    return 0;
}

int main(int   argc,
         char *argv[])
{
    size_t       threads = 1000, i;
    aligned_t   *rets;
    qtimer_t     t;
    unsigned int iter, iterations = 10;
    double       total_time = 0.0;
    double       max_time   = 0.0;
    double       min_time   = DBL_MAX;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags   = SA_SIGINFO;

    // TODO(npe) how does qthreads handle signals?
    sigaction(SIGSEGV, &sa, NULL);

    assert(qthread_initialize() == 0);
    t = qtimer_create();

    CHECK_VERBOSE();
    NUMARG(threads, "THREADS");
    NUMARG(iterations, "ITERATIONS");

    rets = (aligned_t *)malloc(threads * sizeof(aligned_t));
    assert(rets);

    iprintf("creating the barrier for %zu threads\n", threads);

    for (iter = 0; iter <= iterations; ++iter) {
        // iprintf("%i: forking the threads\n", iter);
        for (i = 1; i < threads; ++i) {
            qthread_spawn(segfaults_thread, NULL, NULL,
                          rets + i,
                          0, NULL, i, 0);
        }
    }

    free(rets);

    iprintf("Success!\n");

    return 0;
}

/* vim:set expandtab */
