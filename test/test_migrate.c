#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>

static aligned_t checkres(qthread_t *me, void *arg)
{
    int myshep = qthread_shep(me);

    assert(myshep == 1 || myshep == 0);

    assert(myshep == (int)arg);

    return 0;
}

static aligned_t migrant(qthread_t * me, void *arg)
{
    int myshep = qthread_shep(me);

    assert(myshep == 1 || myshep == 0);

    if (myshep == 1) {
	qthread_migrate_to(me, 0);
	assert(qthread_shep(me) == 0);
	assert(qthread_shep(NULL) == 0);
    } else {
	qthread_migrate_to(me, 1);
	assert(qthread_shep(me) == 1);
	assert(qthread_shep(NULL) == 1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;
    qthread_t *me;

    qthread_init(2);
    me = qthread_self();

    qthread_fork_to(checkres, (void*)0, &ret, 0);
    qthread_readFF(me, &ret, &ret);
    qthread_fork_to(checkres, (void*)1, &ret, 1);
    qthread_readFF(me, &ret, &ret);

    qthread_fork(migrant, NULL, &ret);
    qthread_readFF(me, &ret, &ret);

    qthread_finalize();

    return 0;
}
