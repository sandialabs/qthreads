#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>

static aligned_t x;
static aligned_t id = 1;
static aligned_t readout = 0;

static aligned_t consumer(qthread_t * t, void *arg)
{
    int me;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_readFE(t, &readout, &x);

    return 0;
}

static aligned_t producer(qthread_t * t, void *arg)
{
    int me;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);

    qthread_writeEF_const(t, &x, 55);

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t t;
    int threads = 1;
    int interactive = 0;

    x = 0;
    if (argc == 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0)
	    threads = 1;
	interactive = 1;
    }
    assert(qthread_init(threads) == 0);

    if (interactive == 1) {
	printf("%i threads...\n", threads);
	printf("Initial value of x: %lu\n", (unsigned long)x);
    }

    qthread_fork(consumer, NULL, NULL);
    qthread_fork(producer, NULL, &t);
    qthread_readFF(qthread_self(), &t, &t);

    qthread_finalize();

    if (x == 55) {
	if (interactive == 1) {
	    printf("Success! x==55\n");
	}
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%lu\n", (unsigned long)x);
	return -1;
    }
}
