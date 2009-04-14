#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>

aligned_t master = 0;

aligned_t incr(qthread_t * me, void *arg)
{
    aligned_t localmaster, addition, ret;
    ret = master;
    do {
	localmaster = ret;
	addition = localmaster + 1;
	ret = qthread_cas(&master, ret, addition);
    } while (ret != localmaster);
    return 0;
}

aligned_t incr5(qthread_t * me, void *arg)
{
    qthread_incr(&master, 5);
    return 0;
}

int main()
{
    int i;
    aligned_t rets[30];
    qthread_t *me;

    qthread_init(7);
    me = qthread_self();

    rets[0] = qthread_cas(&master, master, 1);
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
	printf("master is %lu rather than 30\n", (long unsigned)master);
    }

    qthread_finalize();

    return 0;
}
