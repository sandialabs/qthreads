#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>

static aligned_t whereami(qthread_t * me, void *arg)
{
    return qthread_shep(me);
}

int main()
{
    unsigned int i;
    aligned_t rets[30];
    qthread_t *me;
    qthread_shepherd_id_t numsheps = 7;

    qthread_init(numsheps);
    me = qthread_self();
    numsheps = qthread_num_shepherds();

    for (i = 0; i < 30; i++) {
	qthread_fork(whereami, NULL, &(rets[i]));
    }
    for (i = 0; i < 30; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    for (i = 0; i < 30; i++) {
	if (rets[i] != i % numsheps) {
	    printf("rets[%i] = %u ->? %i\n", i, (unsigned int)rets[i], i % numsheps);
	}
	assert(rets[i] == i % numsheps);
    }

    qthread_finalize();

    return 0;
}
