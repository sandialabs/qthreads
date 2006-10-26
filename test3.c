#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "qthread.h"

static volatile int x = 0;
static int id = 1;
static int readout = 0;
pthread_mutex_t alldone = PTHREAD_MUTEX_INITIALIZER;

void conditioner(qthread_t * t)
{
    int me = 0;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_empty(t, (char*)&x, sizeof(x));

    pthread_mutex_unlock(&alldone);
}

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

    pthread_mutex_unlock(&alldone);
}

int main(int argc, char *argv[])
{
    pthread_mutex_lock(&alldone);

    qthread_init(1);

    qthread_fork_detach(conditioner, NULL);

    pthread_mutex_lock(&alldone);
    printf("current value of x: %i\n", x);

    qthread_fork_detach(consumer, NULL);
    qthread_fork_detach(producer, NULL);

    pthread_mutex_lock(&alldone);

    qthread_finalize();

    fprintf(stderr, "Final value of x=%d\n", x);

    return 0;
}
