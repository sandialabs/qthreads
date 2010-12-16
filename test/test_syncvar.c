#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

#define ITERATIONS 10000
#define PAIRS 3

static syncvar_t x = SYNCVAR_STATIC_INITIALIZER;
static syncvar_t id = SYNCVAR_STATIC_INITIALIZER;
static uint64_t readout = 0;

static aligned_t consumer(qthread_t * t, void *arg)
{
    uint64_t me;

    iprintf("consumer(%p) locking id(%p)\n", t, &id);
    qthread_syncvar_readFE(t, &me, &id);
    iprintf("consumer(%p) id's status became: %u\n", t,
	    qthread_syncvar_status(&id));
    iprintf("consumer(%p) unlocking id(%p), result is %lu\n", t, &id,
	    (unsigned long)me);
    me++;
    qthread_syncvar_writeEF(t, &id, &me);

    for (uint64_t i = 0; i < ITERATIONS; ++i) {
	iprintf("consumer(%p) readFF on x\n", t);
	qthread_syncvar_readFE(t, &readout, &x);
    }
    qthread_syncvar_fill(t, &x);

    return 0;
}

static aligned_t producer(qthread_t * t, void *arg)
{
    uint64_t me;

    iprintf("producer(%p) locking id(%p)\n", t, &id);
    qthread_syncvar_readFE(t, &me, &id);
    iprintf("producer(%p) unlocking id(%p), result is %lu\n", t, &id,
	    (unsigned long)me);
    me++;
    qthread_syncvar_writeEF(t, &id, &me);

    for (uint64_t i = 0; i < ITERATIONS; ++i) {
	iprintf("producer(%p) x's status is: %s (expect empty)\n", t,
		qthread_syncvar_status(&x)?"full":"empty");
	iprintf("producer(%p) filling x(%p)\n", t, &x);
	qthread_syncvar_writeEF_const(t, &x, i);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t t[PAIRS][2];
    uint64_t x_value;

    assert(qthread_initialize() == 0);

    CHECK_VERBOSE();

    iprintf("%i threads...\n", qthread_num_shepherds());
    iprintf("Initial value of x: %lu\n", (unsigned long)x.u.w);

    qthread_syncvar_empty(qthread_self(), &id);
    qthread_syncvar_writeF_const(qthread_self(), &id, 1);
    iprintf("id = 0x%lx\n", (unsigned long)id.u.w);
    {
	uint64_t tmp = 0;
	qthread_syncvar_readFF(qthread_self(), &tmp, &id);
	assert(tmp == 1);
    }
    iprintf("x's status is: %s (want full (and nowait))\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    assert(qthread_syncvar_status(&x) == 1);
    qthread_syncvar_readFE(qthread_self(), NULL, &x);
    iprintf("x's status became: %s (want empty (and nowait))\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    assert(qthread_syncvar_status(&x) == 0);
    for (unsigned int i = 0; i < PAIRS; ++i) {
	qthread_fork(consumer, NULL, &(t[i][0]));
    }
    for (unsigned int i = 0; i < PAIRS; ++i) {
	qthread_fork(producer, NULL, &(t[i][1]));
    }
    for (unsigned int i = 0; i < PAIRS; ++i) {
	qthread_readFF(qthread_self(), NULL, &(t[i][0]));
	qthread_readFF(qthread_self(), NULL, &(t[i][1]));
    }
    iprintf("shouldn't be blocking on x (current status: %s)\n",
	    qthread_syncvar_status(&x)?"full":"empty");
    qthread_syncvar_readFF(qthread_self(), &x_value, &x);
    assert(qthread_syncvar_status(&x) == 1);


    if (x_value == 55) {
	iprintf("Success! x==55\n");
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%lu\n", (unsigned long)x_value);
	return -1;
    }
}
