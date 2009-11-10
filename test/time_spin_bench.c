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
#define LOCK_COUNT 10
#define LOCK_ITERS 100

aligned_t counters[LOCK_COUNT] = { 0 };

aligned_t qincr(qthread_t *me, void *arg)
{
    aligned_t id = (aligned_t)arg;
    size_t incrs, iter;

    for (iter = 0; iter < LOCK_ITERS; iter++) {
	for (incrs = 0; incrs < LOCK_COUNT; incrs++) {
	    qthread_lock(me, &(counters[incrs]));
	    while (counters[incrs] != id) {
		qthread_unlock(me, &(counters[incrs]));
		qthread_yield(me);
		qthread_lock(me, &(counters[incrs]));
	    }
	    counters[incrs]++;
	    qthread_unlock(me, &(counters[incrs]));
	}
	id += LOCK_COUNT;
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
	memset(counters, 0, sizeof(aligned_t)*LOCK_COUNT);
	qtimer_start(timer);
	//for (int i=0; i<NUM_THREADS; i++) {
	for (int i=NUM_THREADS-1; i>=0; i--) {
	    qthread_fork(qincr, (void*)(intptr_t)(i), &(rets[i]));
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

    return 0;
}
