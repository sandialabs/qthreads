#include <stdio.h>
#include <stdlib.h>
#include <qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

unsigned long THREADS = 1000000;

static aligned_t null_thread(
    qthread_t * me,
    void *arg)
{
    return 0;
}

int main(
    void)
{
    qtimer_t timer = qtimer_create();
    double spawn;
    aligned_t *rets;
    syncvar_t *srets;
    unsigned int shepherds = 1;

    qthread_initialize();

    CHECK_VERBOSE();
    NUMARG(THREADS, "THREADS");
    shepherds = qthread_num_shepherds();
    printf("%u shepherds...\n", shepherds);
    {
	const size_t bytes =
	    (THREADS * sizeof(ucontext_t) +
	     THREADS * (sizeof(unsigned int) + sizeof(int) +
			sizeof(unsigned char) +
			sizeof(void *) * 11)) / shepherds;
	const size_t kbytes = bytes / 1024;
	const size_t mbytes = kbytes / 1024;
	printf
	    ("With %u shepherd%s, qthread_fork() will need to be able to\n"
	     "allocate %lu threads all at the same time. This will require\n"
	     "at least %lu %s on this machine, and may affect spawn time.\n",
	     shepherds, (shepherds > 1) ? "s" : "", THREADS / shepherds,
	     (mbytes > 0) ? mbytes : ((kbytes > 0) ? kbytes : bytes),
	     (mbytes > 0) ? "MB" : ((kbytes > 0) ? "kB" : "bytes"));
    }

    rets = malloc(sizeof(aligned_t) * THREADS);

    printf("Priming...\n");
    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_fork(null_thread, NULL, rets + i);
    }
    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_readFF(NULL, NULL, rets + i);
    }

    printf("Timing forking...\n");
    qtimer_start(timer);
    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_fork(null_thread, NULL, rets + i);
    }
    qtimer_stop(timer);

    spawn = qtimer_secs(timer);

    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_readFF(NULL, NULL, rets + i);
    }
    qtimer_stop(timer);

    printf("Averaging over %lu threads...\n", THREADS);
    printf("\tTime to spawn aligned_t thread: %g usecs\n",
	   (spawn / THREADS) * 1000000);
    printf("\tTime to sync aligned_t thread:  %g usecs\n",
	   ((qtimer_secs(timer) - spawn) / THREADS) * 1000000);
    free(rets);

    srets = calloc(THREADS, sizeof(syncvar_t));
    qtimer_start(timer);
    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_fork_syncvar(null_thread, NULL, srets + i);
    }
    qtimer_stop(timer);

    spawn = qtimer_secs(timer);

    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_syncvar_readFF(NULL, NULL, srets + i);
    }
    qtimer_stop(timer);

    printf("\tTime to spawn syncvar_t thread: %g usecs\n",
	   (spawn / THREADS) * 1000000);
    printf("\tTime to sync syncvar_t thread:  %g usecs\n",
	   ((qtimer_secs(timer) - spawn) / THREADS) * 1000000);
    free(srets);

    qtimer_start(timer);
    for (unsigned long i = 0; i < THREADS; ++i) {
	qthread_fork(null_thread, NULL, NULL);
    }
    qtimer_stop(timer);
    printf("\tTime to spawn no-ret thread:    %g usecs\n",
	   (qtimer_secs(timer) / THREADS) * 1000000);

    qtimer_destroy(timer);
    return 0;
}
