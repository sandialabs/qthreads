#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>

aligned_t master = 0;

static aligned_t incr(qthread_t * me, void *arg)
{
    qthread_incr(&master, 1);
    return 0;
}

static aligned_t incr5(qthread_t * me, void *arg)
{
    qthread_incr(&master, 5);
    return 0;
}

int main()
{
    int i;
    aligned_t rets[30];
    qthread_t *me;

    qthread_initialize();
    me = qthread_self();

    rets[0] = qthread_incr(&master, 1);
    assert(master == 1);
    assert(rets[0] == 0);
    master = 0;
    for (i = 0; i < 30; i++) {
	qthread_fork(incr, NULL, &(rets[i]));
    }
    for (i = 0; i < 30; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    if (master != 30) {
	fprintf(stderr,"master is %lu rather than 30\n", (long unsigned)master);
    }
    assert(master == 30);
    master = 0;
    for (i = 0; i < 30; i++) {
	qthread_fork(incr5, NULL, &(rets[i]));
    }
    for (i = 0; i < 30; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    if (master != 150) {
	fprintf(stderr,"master is %lu rather than 150\n", (long unsigned)master);
    }
    assert(master == 150);

    return 0;
}
