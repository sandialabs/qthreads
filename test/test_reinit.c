#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static aligned_t x;
static aligned_t id = 1;
static aligned_t readout = 0;

static aligned_t consumer(qthread_t * t, void *arg)
{
    int me;

    iprintf("consumer(%p:%i) locking id(%p)\n", t, qthread_id(t), &id);
    qthread_lock(t, &id);
    me = id++;
    iprintf("consumer(%p:%i) unlocking id(%p), result is %i\n", t,
	    qthread_id(t), &id, me);
    qthread_unlock(t, &id);

    qthread_readFE(t, &readout, &x);

    return 0;
}

static aligned_t producer(qthread_t * t, void *arg)
{
    int me;

    iprintf("producer(%p:%i) locking id(%p)\n", t, qthread_id(t), &id);
    qthread_lock(t, &id);
    me = id++;
    iprintf("producer(%p:%i) unlocking id(%p), result is %i\n", t,
	    qthread_id(t), &id, me);
    qthread_unlock(t, &id);

    iprintf("producer(%p:%i) filling x(%p)\n", t, qthread_id(t), &x);
    qthread_writeEF_const(t, &x, 55);

    return 0;
}

static int realmain(void)
{
    aligned_t t;

    iprintf("%i threads...\n", qthread_num_shepherds());
    iprintf("Initial value of x: %lu\n", (unsigned long)x);

    qthread_fork(consumer, NULL, NULL);
    qthread_fork(producer, NULL, &t);
    qthread_readFF(qthread_self(), &t, &t);


    if (x == 55) {
	iprintf("Success! x==55\n");
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%lu\n", (unsigned long)x);
	return -1;
    }
}

int main(int argc, char *argv[])
{
    assert(qthread_initialize() == 0);

    x = 0;
    CHECK_VERBOSE();

    iprintf("initialized, calling realmain()\n");
    realmain();
    iprintf("finalizing...\n");
    qthread_finalize();
    iprintf("ready to reinitialize!\n");
    qthread_init(1);
    iprintf("reinitialized, calling realmain()\n");
    realmain();
    iprintf("finalizing...\n");
    qthread_finalize();
    iprintf("exiting!\n");
    return 0;
}
