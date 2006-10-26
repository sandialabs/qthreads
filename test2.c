#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "qthread.h"
#include "memwatch.h"

static int target;
static int x = 0;
static int id = 0;

pthread_mutex_t alldone = PTHREAD_MUTEX_INITIALIZER;

void thread(qthread_t * t)
{
    int me = 0;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);
    //printf("thread(%p): me %i\n", (void*) t, me);

    qthread_lock(t, &x);
    //printf("thread(%i): x=%d\n", me, x);
    x++;
    if (x == target) pthread_mutex_unlock(&alldone);
    qthread_unlock(t, &x);
}

int main(int argc, char *argv[])
{
    long int i;

    if (argc != 2) {
	printf("usage: %s num_threads\n", argv[0]);
	return -1;
    } else {
	target = strtol(argv[1], NULL, 0);
    }

    qthread_init(2);

    pthread_mutex_lock(&alldone);

    for (i=0;i<target;i++)
	qthread_fork_detach(thread, NULL);

    pthread_mutex_lock(&alldone);

    qthread_finalize();

    fprintf(stderr, "Final value of x=%d\n", x);

    return 0;
}
