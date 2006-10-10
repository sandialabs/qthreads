#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static volatile int x = 0;
static int id = 0;

void thread(qthread_t * t)
{
    int me = 0;
    int i;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);
    printf("thread(%p): me %i\n", t, me);

    qthread_lock(t, &x);
    printf("thread(%i): x=%d\n", me, x);
    x++;
    qthread_unlock(t, &x);
}

int main(int argc, char *argv[])
{
    int i, target = 128;

    qthread_init(3);

    for (i=0;i<target;i++)
	qthread_fork(thread, NULL);

    while (x != target)
	;

    qthread_finalize();

    printf("Final value of x=%d\n", x);
}
