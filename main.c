#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static int x=0;

void thread(qthread_t *t)
{
    int y;

    qthread_lock(t, &x);
    printf("thread(%p): x=%d\n", t, x);
    x++;
    qthread_unlock(t, &x);
}

int main(int argc, char *argv[])
{
    qthread_t *a, *b, *c, *d;

    qthread_init(3);

    a=qthread_fork(thread, NULL);
    b=qthread_fork(thread, NULL);
    c=qthread_fork(thread, NULL);
    d=qthread_fork(thread, NULL);

    qthread_join(a);
    qthread_join(b);
    qthread_join(c);
    qthread_join(d);

    qthread_finalize();
    
    printf("Final value of x=%d\n", x);
}
