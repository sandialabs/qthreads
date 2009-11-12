#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qlfqueue.h>
#include "argparsing.h"

#define ELEMENT_COUNT 10000
#define THREAD_COUNT 128

static aligned_t queuer (qthread_t *me, void *arg)
{
    qlfqueue_t *q = (qlfqueue_t*)arg;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qlfqueue_enqueue(me, q, (void*)me) != QTHREAD_SUCCESS) {
	    fprintf(stderr, "qlfqueue_enqueue(q, %p) failed!\n", (void*)me);
	    exit(-2);
	}
    }
    return 0;
}

static aligned_t dequeuer (qthread_t *me, void *arg)
{
    qlfqueue_t *q = (qlfqueue_t*)arg;
    size_t i;

    for (i = 0; i < ELEMENT_COUNT; i++) {
	while (qlfqueue_dequeue(me, q) == NULL) {
	    qthread_yield(me);
	}
    }
    return 0;
}

int main(int argc, char *argv[])
{
    qlfqueue_t *q;
    qthread_t *me;
    size_t i;
    aligned_t *rets;

    assert(qthread_initialize() == 0);
    me = qthread_self();
    CHECK_INTERACTIVE();

    if ((q = qlfqueue_create()) == NULL) {
	fprintf(stderr, "qlfqueue_create() failed!\n");
	exit(-1);
    }

    if (qlfqueue_enqueue(me, q, (void*)(intptr_t)1) != 0) {
	fprintf(stderr, "qlfqueue_enqueue() failed!\n");
	exit(-1);
    }

    if (qlfqueue_dequeue(me, q) != (void*)(intptr_t)1) {
	fprintf(stderr, "qlfqueue_dequeue() failed!\n");
	exit(-1);
    }

    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qlfqueue_enqueue(me, q, (void*)(intptr_t)(i+1)) != 0) {
	    fprintf(stderr, "qlfqueue_enqueue(q,%i) failed!\n", (int)i);
	    exit(-1);
	}
    }
    for (i = 0; i < ELEMENT_COUNT; i++) {
	if (qlfqueue_dequeue(me, q) != (void*)(intptr_t)(i+1)) {
	    fprintf(stderr, "qlfqueue_dequeue() failed, didn't equal %i!\n", (int)i);
	    exit(-1);
	}
    }
    if (!qlfqueue_empty(q)) {
	fprintf(stderr, "qlfqueue not empty after ordering test!\n");
	exit(-1);
    }
    iprintf("ordering test succeeded\n");

    rets = calloc(THREAD_COUNT, sizeof(aligned_t));
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
    if (!qlfqueue_empty(q)) {
	fprintf(stderr, "qlfqueue not empty after threaded test!\n");
	exit(-2);
    }
    iprintf("threaded test succeeded\n");

    if (qlfqueue_destroy(me, q) != QTHREAD_SUCCESS) {
	fprintf(stderr, "qlfqueue_destroy() failed!\n");
	exit(-2);
    }

    iprintf("success!\n");

    return 0;
}
