#include <stdio.h>
#include <stdlib.h>
#include <qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

unsigned long THREADS = 1000;

static aligned_t null_thread(qthread_t *me, void *arg)
{
    return 0;
}

int main(void)
{
    qtimer_t timer = qtimer_create();
    double spawn;
    aligned_t *rets;
    //syncvar_t *srets;
    unsigned int shepherds = 1;

    qthread_initialize();

    CHECK_VERBOSE();
    NUMARG(THREADS, "THREADS");
    shepherds = qthread_num_shepherds();
    printf("%u shepherds...\n", shepherds);

    rets = malloc(sizeof(aligned_t) * THREADS);

    qtimer_start(timer);
    for (unsigned long i=0; i<THREADS; ++i) {
	qthread_fork(null_thread, NULL, rets + i);
    }
    qtimer_stop(timer);

    spawn = qtimer_secs(timer);

    for (unsigned long i=0; i<THREADS; ++i) {
	qthread_readFF(NULL, NULL, rets + i);
    }
    qtimer_stop(timer);

    printf("Averaging over %lu threads...\n", THREADS);
    printf("\tTime to spawn: %g usecs\n", (spawn/THREADS)*1000000);
    printf("\tTime to sync:  %g usecs\n", ((qtimer_secs(timer)-spawn)/THREADS)*1000000);
    free(rets);

#if 0
    srets = malloc(sizeof(syncvar_t) * THREADS);
    qtimer_start(timer);
    for (unsigned long i=0; i<THREADS; ++i) {
	qthread_fork_syncvar(null_thread, NULL, srets + i);
    }
    qtimer_stop(timer);

    spawn = qtimer_secs(timer);

    for (unsigned long i=0; i<THREADS; ++i) {
	qthread_syncvar_readFF(NULL, NULL, srets + i);
    }
    qtimer_stop(timer);

    printf("\tTime to spawn: %g usecs\n", (spawn/THREADS)*1000000);
    printf("\tTime to sync:  %g usecs\n", ((qtimer_secs(timer)-spawn)/THREADS)*1000000);
    free(srets);
#endif

    qtimer_destroy(timer);
}
