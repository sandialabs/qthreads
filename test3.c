#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "qthread.h"

static int x __attribute__ ((aligned(8)));
static int id = 1;
static int readout = 0;

void consumer(qthread_t * t, void *arg)
{
    int me = 0;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_readFE(t, &readout, &x);
}

void producer(qthread_t * t, void *arg)
{
    int me = 0;
    int data = 55;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_writeEF(t, &x, &data);
}

int main(int argc, char *argv[])
{
    qthread_t *t;

    x = 0;
    qthread_init(3);

    /*printf("Initial value of x: %i\n", x);*/

    qthread_fork_detach(consumer, NULL);
    t = qthread_fork(producer, NULL);
    qthread_join(NULL, t);

    qthread_finalize();

    if (x == 55)
	return 0;
    else {
	fprintf(stderr, "Final value of x=%d\n", x);
	return -1;
    }
}
