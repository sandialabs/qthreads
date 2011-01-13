#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static aligned_t x;
static aligned_t id = 1;
static aligned_t readout = 0;
static aligned_t t;

static aligned_t consumer(qthread_t * t, void *arg)
{
    int me;

    iprintf("consumer(%p) locking id(%p)\n", t, &id);
    qthread_lock(t, &id);
    me = id++;
    iprintf("consumer(%p) unlocking id(%p), result is %i\n", t, &id, me);
    qthread_unlock(t, &id);

    qthread_readFE(t, &readout, &x);

    return 0;
}

static aligned_t producer(qthread_t * t, void *arg)
{
    int me;

    iprintf("producer(%p) locking id(%p)\n", t, &id);
    qthread_lock(t, &id);
    me = id++;
    iprintf("producer(%p) unlocking id(%p), result is %i\n", t, &id, me);
    qthread_unlock(t, &id);

    iprintf("producer(%p) filling x(%p)\n", t, &x);
    qthread_writeEF_const(t, &x, 55);

    return 0;
}

static void *external_thread(void *junk)
{
    qthread_fork(consumer, NULL, NULL);
    qthread_fork(producer, NULL, &t);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t external;

    assert(qthread_initialize() == 0);

    x = 0;
    CHECK_VERBOSE();

    iprintf("%i threads...\n", qthread_num_shepherds());
    iprintf("Initial value of x: %lu\n", (unsigned long)x);

    pthread_create(&external, NULL, external_thread, NULL);
    pthread_join(external, NULL);

    qthread_readFF(qthread_self(), &t, &t);

    if (x == 55) {
	iprintf("Success! x==55\n");
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%lu (expected 55)\n", (unsigned long)x);
	return -1;
    }
}
