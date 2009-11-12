#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static aligned_t checkres(qthread_t *me, void *arg)
{
    qthread_shepherd_id_t myshep = qthread_shep(me);

    printf("myshep = %i, should be %i\n", myshep, (int)(intptr_t)arg);

    assert(myshep == 1 || myshep == 0 || myshep == 2);

    assert(myshep == (qthread_shepherd_id_t)(intptr_t)arg);

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
	iprintf("migrant starting on %i, aimed at 1, ended up on %i\n", myshep,
		qthread_shep(me));
	if (arg == (void*)2) {
	    assert(qthread_shep(me) != 1);
	} else {
	    assert(qthread_shep(me) == 1);
	}
	assert(qthread_shep(NULL) == qthread_shep(me));
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;
    qthread_t *me;
    int qret;

    putenv("QTHREAD_NUM_SHEPHERDS=3");
    qthread_initialize();
    me = qthread_self();
    qthread_disable_shepherd(1);

    CHECK_INTERACTIVE();

    assert(qthread_num_shepherds() == 3);
    iprintf("now to fork to shepherd 0...\n");
    qret = qthread_fork_to(checkres, (void*)0, &ret, 0);
    assert(qret == QTHREAD_SUCCESS);
    qthread_readFF(me, &ret, &ret);
    iprintf("\tsuccess in forking to shepherd 0!\n");
    iprintf("now to fork to shepherd 1...\n");
    qret = qthread_fork_to(checkres, (void*)1, &ret, 1);
    assert(qret == QTHREAD_NOT_ALLOWED);
    qthread_readFF(me, &ret, &ret);
    iprintf("\tsuccessfully failed to fork to shepherd 1!\n");
    iprintf("now to fork to shepherd 2...\n");
    qret = qthread_fork_to(checkres, (void*)2, &ret, 2);
    assert(qret == QTHREAD_SUCCESS);
    qthread_readFF(me, &ret, &ret);
    iprintf("\tsuccess in forking to shepherd 2!\n");
    iprintf("now to fork the migrant...\n");
    qthread_fork_to(migrant, (void*)2, &ret, 0);
    iprintf("success in forking migrant!\n");
    qthread_readFF(me, &ret, &ret);
    iprintf("migrant returned successfully!\n");
    qthread_enable_shepherd(1);
    iprintf("now to fork the second migrant...\n");
    qthread_fork_to(migrant, (void*)1, &ret, 0);
    iprintf("success in forking second migrant!\n");
    qthread_readFF(me, &ret, &ret);
    iprintf("migrant returned successfully!\n");

    return 0;
}
