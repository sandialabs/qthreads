#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>

static aligned_t checkres(qthread_t *me, void *arg)
{
    qthread_shepherd_id_t myshep = qthread_shep(me);

    assert(myshep == 1 || myshep == 0);

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
	assert(qthread_shep(me) == 1);
	assert(qthread_shep(NULL) == 1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;
    qthread_t *me;
    int interactive = 0;

    qthread_init(2);
    me = qthread_self();

    if (argc >= 2) {
	interactive = 1;
    }

    assert(qthread_num_shepherds() >= 2);
    if (interactive) {
	printf("now to fork to shepherd 0...\n");
	fflush(stdout);
    }
    qthread_fork_to(checkres, (void*)0, &ret, 0);
    qthread_readFF(me, &ret, &ret);
    if (interactive) {
	printf("success in forking to shepherd 0!\n");
	printf("now to fork to shepherd 1...\n");
	fflush(stdout);
    }
    qthread_fork_to(checkres, (void*)1, &ret, 1);
    qthread_readFF(me, &ret, &ret);
    if (interactive) {
	printf("success in forking to shepherd 1!\n");
	printf("now to fork the migrant...\n");
	fflush(stdout);
    }
    qthread_fork(migrant, NULL, &ret);
    if (interactive) {
	printf("success in forking migrant!\n");
	fflush(stdout);
    }
    qthread_readFF(me, &ret, &ret);
    if (interactive) {
	printf("migrant returned successfully!\n");
    }

    qthread_finalize();

    return 0;
}
