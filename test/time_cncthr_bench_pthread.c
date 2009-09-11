#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qtimer.h>

#include <pthread.h>

#define NUM_THREADS 1000000

void * qincr(void *arg)
{
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
    size_t counter;

    for (int iteration = 0; iteration < 10; iteration++) {
	qtimer_start(timer);
	for (int i=0; i<NUM_THREADS; i++) {
	    pthread_create(&(rets[i]), NULL, qincr, &counter);
	}
	for (int i=0; i<NUM_THREADS; i++) {
	    pthread_join(rets[i], NULL);
	}
	qtimer_stop(timer);
	cumulative_time += qtimer_secs(timer);
    }
    printf("pthread time: %f\n", cumulative_time/10.0);

    return 0;
}
