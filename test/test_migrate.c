#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static aligned_t checkres(qthread_t * me, void *arg)
{
    qthread_shepherd_id_t myshep = qthread_shep(me);

    assert(myshep == 1 || myshep == 0);

    assert(myshep == (qthread_shepherd_id_t) (intptr_t) arg);

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

#ifdef __INTEL_COMPILER
int setenv(const char *name, const char *value, int overwrite);
#endif

int main(int argc, char *argv[])
{
    aligned_t ret;
    qthread_t *me;

    setenv("QTHREAD_NUM_SHEPHERDS","2",1);
    qthread_initialize();
    me = qthread_self();

    CHECK_VERBOSE();

    assert(qthread_num_shepherds() == 2);
    iprintf("now to fork to shepherd 0...\n");
    qthread_fork_to(checkres, (void *)0, &ret, 0);
    qthread_readFF(me, &ret, &ret);
    iprintf("success in forking to shepherd 0!\n");
    iprintf("now to fork to shepherd 1...\n");
    qthread_fork_to(checkres, (void *)1, &ret, 1);
    qthread_readFF(me, &ret, &ret);
    iprintf("success in forking to shepherd 1!\n");
    iprintf("now to fork the migrant...\n");
    qthread_fork(migrant, NULL, &ret);
    iprintf("success in forking migrant!\n");
    qthread_readFF(me, &ret, &ret);
    iprintf("migrant returned successfully!\n");

    return 0;
}
