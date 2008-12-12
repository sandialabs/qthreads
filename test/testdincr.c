#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>

double master = 0.0;
double retvals[30];

aligned_t incr(qthread_t * me, void *arg)
{
    retvals[(intptr_t)arg] = qthread_dincr(&master, 1.0);
    return 0;
}

aligned_t incr5(qthread_t * me, void *arg)
{
    qthread_dincr(&master, 5.0);
    return 0;
}

int main()
{
    int i;
    aligned_t rets[30];
    double ret_test;
    qthread_t *me;

    qthread_init(7);
    me = qthread_self();

    ret_test = qthread_dincr(&master, 1);
    if (master != 1.0) {
	printf("master = %f\n", master);
    }
    assert(master == 1.0);
    if (ret_test != 0.0) {
	printf("ret_test = %f\n", ret_test);
    }
    assert(ret_test == 0.0);
    master = 2;
    for (i = 0; i < 30; i++) {
	qthread_fork(incr, (void*)(intptr_t)i, &(rets[i]));
    }
    for (i = 0; i < 30; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    if (master != 32.0) {
	printf("master is %f rather than 32\n", master);
	for (int i=0; i<30; i++) {
	    printf("retvals[%i] = %f\n", i, retvals[i]);
	}
    }
    assert(master == 32.0);
    master = 0.0;
    for (i = 0; i < 30; i++) {
	qthread_fork(incr5, NULL, &(rets[i]));
    }
    for (i = 0; i < 30; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    if (master != 150.0) {
	printf("master is %f rather than 150\n", master);
    }
    assert(master == 150.0);

    qthread_finalize();

    return 0;
}
