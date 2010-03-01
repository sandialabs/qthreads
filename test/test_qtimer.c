#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

int main()
{
    int i;
    qthread_t *me;
    qtimer_t t;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    me = qthread_self();

    CHECK_VERBOSE();

    t = qtimer_create();
    assert(t);
    qtimer_start(t);
    me = qthread_self();
    qtimer_stop(t);
    assert(qtimer_secs(t) > 0);
    iprintf("t = %f secs\n", qtimer_secs(t));
    qtimer_destroy(t);

    return 0;
}
