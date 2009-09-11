#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qtimer.h>

#include <pthread.h>

#define NUM_THREADS 10
#define PER_THREAD_INCR 1000000

aligned_t counters[PER_THREAD_INCR] = { 0 };

aligned_t qincr(qthread_t *me, void *arg)
{
    aligned_t *c = (aligned_t*)arg;
    size_t incrs;

    for (incrs = 0; incrs < PER_THREAD_INCR; incrs++) {
	qthread_lock(me, &(c[incrs]));
	qthread_unlock(me, &(c[incrs]));
    }
}

int main(int argc, char *argv[])
{
    aligned_t rets[NUM_THREADS];
    size_t i;
    int threads = 0;
    int interactive = 0;
    qthread_t *me;
    qtimer_t timer = qtimer_new();
    double cumulative_time = 0.0;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	} else {
	    printf("threads: %i\n", threads);
	}
    }
    interactive = (argc > 2);

    if (qthread_init(threads) != QTHREAD_SUCCESS) {
	fprintf(stderr, "qthread library could not be initialized!\n");
	exit(EXIT_FAILURE);
    }
    me = qthread_self();

    for (int iteration = 0; iteration < 10; iteration++) {
	qtimer_start(timer);
	for (int i=0; i<NUM_THREADS; i++) {
	    qthread_fork(qincr, counters, &(rets[i]));
	}
	for (int i=0; i<NUM_THREADS; i++) {
	    qthread_readFF(me, NULL, &(rets[i]));
	}
	qtimer_stop(timer);
	if (interactive) {
	    printf("\ttest iteration %i: %f secs\n", iteration, qtimer_secs(timer));
	}
	cumulative_time += qtimer_secs(timer);
    }
    printf("qthread time: %f\n", cumulative_time/10.0);

    qthread_finalize();
    return 0;
}
