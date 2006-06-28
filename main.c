#include <stdio.h>
#include <stdlib.h>
#include "qthread.h"

void thread(qthread_t *t, void *arg)
{
    static int x=0;

    qthread_lock(t, &x);
    x++;
    printf("thread(): tid=%d x=%d\n", qthread_get_id(t), x);
    qthread_unlock(t, &x);
}

int main(int argc, char *argv[])
{
    qthread_init(2);

    qthread_fork(thread, NULL);
    qthread_fork(thread, NULL);
    qthread_fork(thread, NULL);

    qthread_print_queue();

    qthread_finalize();
}
