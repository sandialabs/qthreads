#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "qthread.h"

static int x = 0;
static int id = 1;
static int readout = 0;

void consumer(qthread_t * t)
{
    int me = 0;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_readFE_size(t, (char*)&readout, (char*)&x, sizeof(me));
}

void producer(qthread_t * t)
{
    int me = 0;
    int data = 55;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_writeEF_size(t, (char*)&x, (char*)&data, sizeof(me));
}

int main(int argc, char *argv[])
{
    qthread_t *t;

    qthread_init(3);

    printf("Initial value of x: %i\n", x);

    qthread_fork_detach(consumer, NULL);
    t = qthread_fork(producer, NULL);
    qthread_join(NULL, t);

    qthread_finalize();

    fprintf(stderr, "Final value of x=%d\n", x);

    return 0;
}
