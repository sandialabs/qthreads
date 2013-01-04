#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/barrier.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

aligned_t     initme_idx = 1;
aligned_t    *initme     = NULL;
qt_barrier_t *wait_on_me;

static aligned_t barrier_thread(void *arg)
{
    void        **args = (void **)arg;
    qt_barrier_t *b    = (qt_barrier_t *)args[0];
    aligned_t     idx  = qthread_incr(&initme_idx, 1);

    qthread_incr(&(initme[idx]), 1);
    qt_barrier_enter_id(b, (uintptr_t)args[1]);
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

    assert(qthread_initialize() == 0);
    t = qtimer_create();

    CHECK_VERBOSE();
    NUMARG(threads, "THREADS");
    NUMARG(iterations, "ITERATIONS");

    initme = (aligned_t *)calloc(threads, sizeof(aligned_t));
    assert(initme);

    rets = (aligned_t *)malloc(iterations * threads * sizeof(aligned_t));
    assert(rets);

    iprintf("creating the barrier for %zu threads\n", threads);
    wait_on_me = qt_barrier_create(threads, REGION_BARRIER);    // all my spawnees plus me
    assert(wait_on_me);

    for (iter = 0; iter < iterations; iter++) {
        // iprintf("%i: forking the threads\n", iter);
        for (i = 1; i < threads; i++) {
            void *args[2] = { wait_on_me, (void *)(uintptr_t)i };
            qthread_spawn(barrier_thread, args, sizeof(args),
                          rets + (iter * threads) + i,
                          0, NULL, i, 0);
        }
        qthread_flushsc();
        // iprintf("%i: done forking the threads, entering the barrier\n", iter);
        qtimer_start(t);
        qt_barrier_enter_id(wait_on_me, 0);
        qtimer_stop(t);
        // iprintf("%i: main thread exited barrier in %f seconds\n", iter, qtimer_secs(t));
        total_time += qtimer_secs(t);

        initme_idx = 1;

        for (i = 1; i < threads; i++) {
            if (initme[i] != iter + 1) {
                iprintf("initme[%i] = %i (should be %i)\n", (int)i,
                        (int)initme[i], iter + 1);
            }
            assert(initme[i] == iter + 1);
        }
    }

    iprintf("Average barrier time: %f secs\n", total_time / iterations);

    iprintf("Destroying barrier...\n");
    qt_barrier_destroy(wait_on_me);

    iprintf("Success!\n");

    /* this loop shouldn't be necessary... but seems to avoid crashes in rare
     * cases (in other words there must a race condition in qthread_finalize()
     * if there are outstanding threads out there) */
    for (i = 0; i < threads * iterations; i++) {
        aligned_t tmp = 1;
        qthread_readFF(&tmp, rets + i);
        assert(tmp == 0);
    }
    return 0;
}

/* vim:set expandtab */
