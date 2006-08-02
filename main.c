#include <stdio.h>
#include <assert.h>
#include "qthread.h"

#define NKTHREAD        3
#define NTHREAD         10

void thread(qthread_t *t)
{
    int *arg;

    arg = (int *)qthread_get_arg(t);
    printf("thread(%p/%d): arg 0x%p = %d forked\n", t, t->thread_id, arg, *arg);
}

int main(int argc, char *argv[])
{
    int i, shep, args[NTHREAD];
    qthread_t *t;

    qthread_init(NKTHREAD);

    for(i=0; i<NTHREAD; i++) {
        args[i] = i;
        qthread_fork(thread, (void *)&args[i]);
    }

    qthread_finalize();
}
