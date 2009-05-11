#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>

static aligned_t migrant(qthread_t * me, void *arg)
{
    int myshep = qthread_shep(me);

    assert(myshep == 1 || myshep == 0);

    if (myshep == 1) {
	qthread_migrate_to(me, 0);
	assert(qthread_shep(me) == 0);
    } else {
	qthread_migrate_to(me, 1);
	assert(qthread_shep(me) == 1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;

    qthread_init(2);

    qthread_fork(migrant, NULL, &ret);
    qthread_readFF(qthread_self(), &ret, &ret);

    qthread_finalize();

    return 0;
}
