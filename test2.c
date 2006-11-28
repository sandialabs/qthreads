#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qthread.h>

static int target;
static int x = 0;
static int id = 0;

pthread_mutex_t alldone = PTHREAD_MUTEX_INITIALIZER;

void thread(qthread_t * t, void *arg)
{
    int me = 0;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);
    //printf("thread(%p): me %i\n", (void*) t, me);

    qthread_lock(t, &x);
    //printf("thread(%i): x=%d\n", me, x);
    x++;
    if (x == target)
	pthread_mutex_unlock(&alldone);
    qthread_unlock(t, &x);
}

int main(int argc, char *argv[])
{
    long int i;
    int interactive;

    if (argc != 2) {
	target = 1000;
	interactive = 0;
    } else {
	target = strtol(argv[1], NULL, 0);
	interactive = 1;
    }

    qthread_init(2);

    pthread_mutex_lock(&alldone);

    for (i = 0; i < target; i++)
	qthread_fork_detach(thread, NULL);

    pthread_mutex_lock(&alldone);

    qthread_finalize();

    if (interactive == 1) {
	fprintf(stderr, "Final value of x=%d\n", x);
    }

    if (x == target)
	return 0;
    else
	return -1;
}
