#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static syncvar_t x = SYNCVAR_STATIC_INITIALIZER;
static syncvar_t id = SYNCVAR_STATIC_INITIALIZER;
static uint64_t iterations = 10000;

static aligned_t consumer(void *arg)
{
    qthread_t *t = qthread_self();
    uint64_t me;

    iprintf("consumer(%p) locking id(%p)\n", t, &id);
    qthread_syncvar_readFE(&me, &id);
    iprintf("consumer(%p) id's status became: %u\n", t,
	    qthread_syncvar_status(&id));
    iprintf("consumer(%p) unlocking id(%p), result is %lu\n", t, &id,
	    (unsigned long)me);
    me++;
    qthread_syncvar_writeEF(&id, &me);

    for (uint64_t i = 0; i < iterations; ++i) {
	iprintf("consumer(%p) readFE on x\n", t);
	qthread_syncvar_readFE(NULL, &x);
    }

    iprintf("thread %i (%p) exiting\n", (int)(uintptr_t)arg, t);

    return 0;
}

static aligned_t producer(void *arg)
{
    qthread_t *t = qthread_self();
    uint64_t me;

    iprintf("producer(%p) locking id(%p)\n", t, &id);
    qthread_syncvar_readFE(&me, &id);
    iprintf("producer(%p) unlocking id(%p), result is %lu\n", t, &id,
	    (unsigned long)me);
    me++;
    qthread_syncvar_writeEF(&id, &me);

    for (uint64_t i = 0; i < iterations; ++i) {
	iprintf("producer(%p) x's status is: %s (expect empty)\n", t,
		qthread_syncvar_status(&x)?"full":"empty");
	iprintf("producer(%p) filling x(%p)\n", t, &x);
	qthread_syncvar_writeEF_const(&x, i);
    }
    iprintf("thread %i (%p) exiting\n", (int)(uintptr_t)arg, t);

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t *t[2];
    uint64_t x_value;
    qthread_t *me;

    uint64_t pairs;

    assert(qthread_initialize() == 0);
    me = qthread_self();
    pairs = qthread_num_shepherds() * 6;

    CHECK_VERBOSE();
    NUMARG(iterations, "ITERATIONS");
    NUMARG(pairs, "PAIRS");

    t[0] = calloc(pairs, sizeof(aligned_t));
    t[1] = calloc(pairs, sizeof(aligned_t));

    iprintf("%i threads...\n", qthread_num_shepherds());
    iprintf("Initial value of x: %lu\n", (unsigned long)x.u.w);

    qthread_syncvar_empty(&id);
    qthread_syncvar_writeF_const(&id, 1);
    iprintf("id = 0x%lx\n", (unsigned long)id.u.w);
    {
	uint64_t tmp = 0;
	qthread_syncvar_readFF(&tmp, &id);
	assert(tmp == 1);
    }
    iprintf("x's status is: %s (want full (and nowait))\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    assert(qthread_syncvar_status(&x) == 1);
    qthread_syncvar_readFE(NULL, &x);
    iprintf("x's status became: %s (want empty (and nowait))\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    assert(qthread_syncvar_status(&x) == 0);
    for (unsigned int i = 0; i < pairs; ++i) {
	qthread_fork(consumer, (void*)(uintptr_t)i, &(t[0][i]));
    }
    for (unsigned int i = 0; i < pairs; ++i) {
	qthread_fork(producer, (void*)(uintptr_t)(i+pairs), &(t[1][i]));
    }
    for (unsigned int i = 0; i < pairs; ++i) {
	qthread_readFF(me, NULL, &(t[0][i]));
	qthread_readFF(me, NULL, &(t[1][i]));
    }
    iprintf("shouldn't be blocking on x (current status: %s)\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    qthread_syncvar_fill(&x);
    iprintf("shouldn't be blocking on x (current status: %s)\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    qthread_syncvar_readFF(&x_value, &x);
    assert(qthread_syncvar_status(&x) == 1);

    free(t[0]);
    free(t[1]);

    if (x_value == iterations-1) {
	iprintf("Success! x==%lu\n", (unsigned long)x_value);
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%lu, expected %lu\n", (unsigned long)x_value, (unsigned long)(iterations-1));
	return -1;
    }
}
