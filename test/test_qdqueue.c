#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qdqueue.h>
#include "argparsing.h"

static unsigned int ELEMENT_COUNT = 10000;
static unsigned int THREAD_COUNT = 128;

static aligned_t queuer(qthread_t * me, void *arg)
{
    qdqueue_t *q = (qdqueue_t *) arg;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qdqueue_enqueue(me, q, (void *)me) != QTHREAD_SUCCESS) {
	    fprintf(stderr, "qdqueue_enqueue(q, %p) failed!\n", (void *)me);
	    exit(-2);
	}
    }
    return 0;
}

static aligned_t dequeuer(qthread_t * me, void *arg)
{
    qdqueue_t *q = (qdqueue_t *) arg;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	while (qdqueue_dequeue(me, q) == NULL) {
	    qthread_yield(me);
	}
    }
    return 0;
}

int main(int argc, char *argv[])
{
    qdqueue_t *q;
    qthread_t *me;
    size_t i;
    aligned_t *rets;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    me = qthread_self();
    CHECK_VERBOSE();
    NUMARG(ELEMENT_COUNT, "ELEMENT_COUNT");
    NUMARG(THREAD_COUNT, "THREAD_COUNT");

    if ((q = qdqueue_create()) == NULL) {
	fprintf(stderr, "qdqueue_create() failed!\n");
	exit(-1);
    }

    if (qdqueue_enqueue(me, q, (void *)me) != 0) {
	fprintf(stderr, "qdqueue_enqueue() failed!\n");
	exit(-1);
    }

    {
	void *ret;

	if ((ret = qdqueue_dequeue(me, q)) != me) {
	    fprintf(stderr, "qdqueue_dequeue() failed! %p\n", ret);
	    exit(-1);
	}
    }

    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qdqueue_enqueue(me, q, (void *)(intptr_t) (i + 1)) != 0) {
	    fprintf(stderr, "qdqueue_enqueue(q,%i) failed!\n", (int)i);
	    exit(-1);
	}
    }
    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qdqueue_dequeue(me, q) != (void *)(intptr_t) (i + 1)) {
	    fprintf(stderr, "qdqueue_dequeue() failed, didn't equal %i!\n",
		    (int)i);
	    exit(-1);
	}
    }
    if (!qdqueue_empty(me, q)) {
	fprintf(stderr, "qdqueue not empty after ordering test!\n");
	exit(-1);
    }
    iprintf("ordering test succeeded\n");

    rets = (aligned_t *) calloc(THREAD_COUNT, sizeof(aligned_t));
    assert(rets != NULL);
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_fork(dequeuer, q, &(rets[i])) == QTHREAD_SUCCESS);
    }
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_fork(queuer, q, NULL) == QTHREAD_SUCCESS);
    }
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_readFF(me, NULL, &(rets[i])) == QTHREAD_SUCCESS);
    }
    free(rets);
    if (!qdqueue_empty(me, q)) {
	fprintf(stderr, "qdqueue not empty after threaded test!\n");
	exit(-2);
    }
    iprintf("threaded test succeeded\n");

    if (qdqueue_destroy(me, q) != QTHREAD_SUCCESS) {
	fprintf(stderr, "qdqueue_destroy() failed!\n");
	exit(-2);
    }

    iprintf("success!\n");

    return 0;
}
