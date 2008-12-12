#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qthread/qthread.h>
#include <qthread_innards.h> /* for qthread_debug() */

aligned_t counter = 0;

aligned_t thread (qthread_t *me, void *arg)
{
    int id = qthread_id(me);
    int ret;
    int ret2;
    //printf("first id = %i\n", id);
#ifdef QTHREAD_DEBUG
    if (id != 1)
	qthread_debug(0, "id == %i\n", id);
#endif
    assert(id == 1);

    ret = qthread_incr(&counter, 1);
    //printf("first inc = %i\n", ret);
    assert(ret == 0);

    ret2 = qthread_incr(&counter, 1);
    //printf("second inc = %i\n", ret2);
    assert(ret2 == 1);
    return 0;
}

int main()
{
    aligned_t ret;
    qthread_t *me;
    int my_id;

    qthread_init(1);
    me = qthread_self();
    my_id = qthread_id(me);
    assert(my_id == 0);
    qthread_fork(thread, NULL, &ret);
    qthread_readFF(qthread_self(), NULL, &ret);
    qthread_finalize();
    return my_id;
}
