#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>

aligned_t master = 0;

aligned_t incr (qthread_t *me, void * arg)
{
    qthread_incr(&master, 1);
    return 0;
}

aligned_t incr5 (qthread_t *me, void* arg)
{
    qthread_incr(&master, 5);
    return 0;
}

int main()
{
    int i;
    aligned_t rets[30];

    qthread_init(7);

    qthread_incr(&master, 1);
    assert(master == 1);
    master = 0;
    for (i=0; i<30; i++) {
	qthread_fork(incr, NULL, &(rets[i]));
    }
    for (i=0;i<30;i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    if (master != 30) {
	printf("master is %lu rather than 30\n", master);
    }
    assert (master == 30);
    master = 0;
    for (i=0; i<30; i++) {
	qthread_fork(incr5, NULL, &(rets[i]));
    }
    for (i=0;i<30;i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    if (master != 150) {
	printf("master is %lu rather than 150\n", master);
    }
    assert (master == 150);

    qthread_finalize();

    return 0;
}
