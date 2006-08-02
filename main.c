#include <stdio.h>
#include <stdlib.h>
#include "qthread.h"

static int x=0;

void thread(qthread_t *t, void *arg)
{
    printf("THREAD(%p, %p): running\n", t, arg);
    qthread_lock(t, &x);
    x++;
    printf("THREAD(%p, %p): tid=%d x=%d\n", t, arg, qthread_get_id(t), x);
    qthread_unlock(t, &x);
    printf("THREAD(%p, %p): finished\n", t, arg);
}

int main(int argc, char *argv[])
{
    qthread_t *me;

    me = qthread_init(2);

    qthread_lock(me, &x);

    qthread_fork(thread, NULL);
    qthread_fork(thread, NULL);
    qthread_fork(thread, NULL);

    printf("Threads forked!\n");

    printf("Releasing lock on x!\n");
    qthread_unlock(me, &x);

    qthread_finalize();

    printf("x=%d\n", x);
}
