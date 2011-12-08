#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qlfqueue.h>
#include <qthread/qt_sinc.h>
#include "argparsing.h"

static size_t elementcount = 10000;
static size_t threadcount = 128;
static qt_sinc_t *dequeue_sinc = NULL;

static aligned_t queuer(void *arg)
{
    qlfqueue_t *q = (qlfqueue_t *)arg;
    size_t i;

    for (i = 0; i < elementcount; i++) {
        if (qlfqueue_enqueue(q, (void *)((intptr_t)qthread_id() + 1)) !=
            QTHREAD_SUCCESS) {
            fprintf(stderr, "qlfqueue_enqueue(q, %p) failed! (%i)\n",
                    (void *)((intptr_t)qthread_id() + 1), __LINE__);
            exit(-2);
        }
    }
    return 0;
}

static aligned_t dequeuer(void *arg)
{
    qlfqueue_t *q = (qlfqueue_t *)arg;
    size_t i;

    for (i = 0; i < elementcount; i++) {
        while (qlfqueue_dequeue(q) == NULL) {
            qthread_yield();
        }
    }
    qt_sinc_submit(dequeue_sinc, NULL);
    iprintf("dequeuer %i exiting\n", qthread_id());
    return 0;
}

int main(int argc,
         char *argv[])
{
    qlfqueue_t *q;
    size_t i;

    assert(qthread_initialize() == 0);
    NUMARG(threadcount, "THREAD_COUNT");
    NUMARG(elementcount, "ELEMENT_COUNT");
    CHECK_VERBOSE();
    iprintf("%i shepherds\n", qthread_num_shepherds());

    if ((q = qlfqueue_create()) == NULL) {
        fprintf(stderr, "qlfqueue_create() failed!\n");
        exit(-1);
    }

    if (qlfqueue_enqueue(q, (void *)(intptr_t)1) != 0) {
        fprintf(stderr, "qlfqueue_enqueue() failed!\n");
        exit(-1);
    }

    if (qlfqueue_dequeue(q) != (void *)(intptr_t)1) {
        fprintf(stderr, "qlfqueue_dequeue() failed!\n");
        exit(-1);
    }

    for (i = 0; i < elementcount; i++) {
        if (qlfqueue_enqueue(q, (void *)(intptr_t)(i + 1)) != 0) {
            fprintf(stderr, "qlfqueue_enqueue(q,%i) failed!\n", (int)i);
            exit(-1);
        }
    }
    for (i = 0; i < elementcount; i++) {
        if (qlfqueue_dequeue(q) != (void *)(intptr_t)(i + 1)) {
            fprintf(stderr, "qlfqueue_dequeue() failed, didn't equal %i!\n",
                    (int)i);
            exit(-1);
        }
    }
    if (!qlfqueue_empty(q)) {
        fprintf(stderr, "qlfqueue not empty after ordering test!\n");
        exit(-1);
    }
    iprintf("ordering test succeeded\n");

    dequeue_sinc = qt_sinc_create(0, NULL, NULL, threadcount);
    assert(dequeue_sinc != NULL);
    for (i = 0; i < threadcount; i++) {
        assert(qthread_fork(dequeuer, q, NULL) == QTHREAD_SUCCESS);
    }
    iprintf("dequeuers forked\n");
    for (i = 0; i < threadcount; i++) {
        assert(qthread_fork(queuer, q, NULL) == QTHREAD_SUCCESS);
    }
    iprintf("queuers forked\n");
    qt_sinc_wait(dequeue_sinc, NULL);
    iprintf("dequeuers returned\n");

    if (!qlfqueue_empty(q)) {
        fprintf(stderr, "qlfqueue not empty after threaded test!\n");
        exit(-2);
    }
    iprintf("threaded test succeeded\n");

    if (qlfqueue_destroy(q) != QTHREAD_SUCCESS) {
        fprintf(stderr, "qlfqueue_destroy() failed!\n");
        exit(-2);
    }

    iprintf("success!\n");

    return 0;
}

/* vim:set expandtab */
