#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qlfqueue.h>
#include "qtimer.h"

#define ELEMENT_COUNT 10000
#define THREAD_COUNT 128

aligned_t queuer (qthread_t *me, void *arg)
{
    qlfqueue_t *q = (qlfqueue_t*)arg;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qlfqueue_enqueue(q, (void*)me) != QTHREAD_SUCCESS) {
	    fprintf(stderr, "qlfqueue_enqueue(q, %p) failed!\n", me);
	    exit(-2);
	}
    }
    return 0;
}

aligned_t dequeuer (qthread_t *me, void *arg)
{
    qlfqueue_t *q = (qlfqueue_t*)arg;
    void *ret;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	while (qlfqueue_dequeue(q) == NULL) {
	    qthread_yield(me);
	}
    }
    return 0;
}

void loop_queuer (qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t i;
    qlfqueue_t *q = (qlfqueue_t *)arg;

    for (i=startat; i<stopat; i++) {
	if (qlfqueue_enqueue(q, (void*)me) != QTHREAD_SUCCESS) {
	    fprintf(stderr, "qlfqueue_enqueue(q, %p) failed!\n", me);
	    exit(-2);
	}
    }
}

void loop_dequeuer (qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t i;
    qlfqueue_t *q = (qlfqueue_t *)arg;

    for (i=startat; i<stopat; i++) {
	if (qlfqueue_dequeue(q) == NULL) {
	    fprintf(stderr, "qlfqueue_dequeue(q, %p) failed!\n", me);
	    exit(-2);
	}
    }
}

int main(int argc, char *argv[])
{
    qlfqueue_t *q;
    int threads = 1, interactive = 0;
    qthread_t *me;
    size_t i;
    aligned_t *rets;
    qtimer_t timer = qtimer_new();

    if (argc == 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads <= 0) {
	    threads = 1;
	    interactive = 0;
	} else {
	    interactive = 1;
	}
    }

    assert(qthread_init(threads) == 0);
    me = qthread_self();

    if ((q = qlfqueue_new()) == NULL) {
	fprintf(stderr, "qlfqueue_new() failed!\n");
	exit(-1);
    }

    /* prime the pump */
    qt_loop_balance(0, THREAD_COUNT * ELEMENT_COUNT, loop_queuer, q);
    qt_loop_balance(0, THREAD_COUNT * ELEMENT_COUNT, loop_dequeuer, q);
    if (!qlfqueue_empty(q)) {
	fprintf(stderr, "qlfqueue not empty after priming!\n");
	exit(-2);
    }

    qtimer_start(timer);
    qt_loop_balance(0, THREAD_COUNT * ELEMENT_COUNT, loop_queuer, q);
    qtimer_stop(timer);
    printf("loop balance enqueue: %f secs\n", qtimer_secs(timer));
    qtimer_start(timer);
    qt_loop_balance(0, THREAD_COUNT * ELEMENT_COUNT, loop_dequeuer, q);
    qtimer_stop(timer);
    printf("loop balance dequeue: %f secs\n", qtimer_secs(timer));
    if (!qlfqueue_empty(q)) {
	fprintf(stderr, "qlfqueue not empty after loop balance test!\n");
	exit(-2);
    }

    rets = calloc(THREAD_COUNT, sizeof(aligned_t));
    assert(rets != NULL);
    qtimer_start(timer);
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_fork(dequeuer, q, &(rets[i])) == QTHREAD_SUCCESS);
    }
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_fork(queuer, q, NULL) == QTHREAD_SUCCESS);
    }
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_readFF(me, NULL, &(rets[i])) == QTHREAD_SUCCESS);
    }
    qtimer_stop(timer);
    free(rets);
    if (!qlfqueue_empty(q)) {
	fprintf(stderr, "qlfqueue not empty after threaded test!\n");
	exit(-2);
    }
    printf("threaded test: %f secs\n", qtimer_secs(timer));

    if (qlfqueue_free(q) != QTHREAD_SUCCESS) {
	fprintf(stderr, "qlfqueue_free() failed!\n");
	exit(-2);
    }

    if (interactive) {
	printf("success!\n");
    }

    qthread_finalize();
    return 0;
}
