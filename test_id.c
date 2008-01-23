#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qthread/qthread.h>

aligned_t counter = 0;

aligned_t thread (qthread_t *me, void *arg)
{
    int id = qthread_id(me);
    int ret;
    int ret2;
    printf("first id = %i\n", id);
    assert(id == 0);

    ret = qthread_incr(&counter, 1);
    printf("first inc = %i\n", ret);
    assert(ret == 0);

    ret2 = qthread_incr(&counter, 1);
    printf("second inc = %i\n", ret2);
    assert(ret2 == 1);
    return 0;
}

int main()
{
    aligned_t ret;
    qthread_init(1);
    qthread_fork(thread, NULL, &ret);
    qthread_readFF(NULL, NULL, &ret);
    qthread_finalize();
    return 0;
}
