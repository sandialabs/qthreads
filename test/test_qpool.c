#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qpool.h>
#include "argparsing.h"

static unsigned int ELEMENT_COUNT = 10000;
static unsigned int THREAD_COUNT = 128;

qpool *qp = NULL;

static aligned_t allocator(qthread_t * me, void *arg)
{
    aligned_t *block[5];
    aligned_t i;
    qpool *p = (qpool *) arg;

    for (i = 0; i < 5; i++) {
	if ((block[i] = (aligned_t *) qpool_alloc(me, p)) == NULL) {
	    fprintf(stderr, "qpool_alloc() failed!\n");
	    exit(-2);
	}
    }
    for (i = 0; i < 5; i++) {
	block[i][0] = i;
    }
    for (i = 0; i < 5; i++) {
	qpool_free(me, p, block[i]);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    qthread_t *me;
    size_t i;
    aligned_t *rets;
    aligned_t **allthat;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    me = qthread_self();
    CHECK_VERBOSE();
    NUMARG(ELEMENT_COUNT, "ELEMENT_COUNT");
    NUMARG(THREAD_COUNT, "THREAD_COUNT");

    if ((qp = qpool_create(sizeof(aligned_t))) == NULL) {
	fprintf(stderr, "qpool_create() failed!\n");
	exit(-1);
    }

    if ((rets = (aligned_t *) qpool_alloc(me, qp)) == NULL) {
	fprintf(stderr, "qpool_alloc() failed!\n");
	exit(-1);
    }
    iprintf("allocated: %p (%lu)\n", (void *)rets, (unsigned long)*rets);
    *rets = 1;
    if (*rets != 1) {
	fprintf(stderr,
		"assigning a value to the allocated memory failed!\n");
	exit(-1);
    }

    qpool_free(me, qp, rets);

    allthat = (aligned_t **) malloc(sizeof(aligned_t *) * ELEMENT_COUNT);
    assert(allthat != NULL);
    for (i = 0; i < ELEMENT_COUNT; i++) {
	if ((allthat[i] = (aligned_t *) qpool_alloc(me, qp)) == NULL) {
	    fprintf(stderr, "qpool_alloc() failed!\n");
	    exit(-2);
	}
    }
    for (i = 0; i < ELEMENT_COUNT; i++) {
	qpool_free(me, qp, allthat[i]);
    }
    free(allthat);

    rets = (aligned_t *) malloc(sizeof(aligned_t) * THREAD_COUNT);
    assert(rets != NULL);
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_fork(allocator, qp, &(rets[i])) == QTHREAD_SUCCESS);
    }
    for (i = 0; i < THREAD_COUNT; i++) {
	assert(qthread_readFF(me, NULL, &(rets[i])) == QTHREAD_SUCCESS);
    }
    free(rets);

    qpool_destroy(qp);

    iprintf("success!\n");
    return 0;
}
