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
pthread_mutex_t counter_locks[LOCK_COUNT];

void* qincr(void *arg)
{
    aligned_t id = (aligned_t)arg;
    size_t incrs, iter;

    for (iter=0; iter<LOCK_ITERS; iter++) {
	for (incrs = 0; incrs < LOCK_COUNT; incrs++) {
	    pthread_mutex_lock(&(counter_locks[incrs]));
	    while (counters[incrs] != id) {
		pthread_mutex_unlock(&(counter_locks[incrs]));
		pthread_mutex_lock(&(counter_locks[incrs]));
	    }
	    counters[incrs]++;
	    pthread_mutex_unlock(&(counter_locks[incrs]));
	}
	id += LOCK_COUNT;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t rets[NUM_THREADS];
    size_t i;
    int threads = 0;
    int interactive = 0;
    qthread_t *me;
    qtimer_t timer = qtimer_new();
    double cumulative_time = 0.0;

    interactive = (argc > 1);

    for (int i=0; i<LOCK_COUNT; i++) {
	pthread_mutex_init(&(counter_locks[i]), NULL);
    }
    for (int iteration = 0; iteration < 10; iteration++) {
	memset(counters, 0, sizeof(aligned_t)*LOCK_COUNT);
	qtimer_start(timer);
	for (int i=0; i<NUM_THREADS; i++) {
	    pthread_create(&(rets[i]), NULL, qincr, (void*)(intptr_t)(i));
	}
	for (int i=0; i<NUM_THREADS; i++) {
	    pthread_join(rets[i], NULL);
	}
	qtimer_stop(timer);
	if (interactive) {
	    printf("\ttest iteration %i: %f secs\n", iteration, qtimer_secs(timer));
	}
	cumulative_time += qtimer_secs(timer);
    }
    printf("pthread time: %f\n", cumulative_time/10.0);

    return 0;
}
