#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

int main()
{
    qtimer_t t;
    qthread_t *me;

    assert(qthread_initialize() == QTHREAD_SUCCESS);

    CHECK_VERBOSE();

    t = qtimer_create();
    assert(t);
    qtimer_start(t);
    me = qthread_self();
    assert(me);
    qtimer_stop(t);
    assert(qtimer_secs(t) > 0);
    iprintf("t = %f secs\n", qtimer_secs(t));
    qtimer_destroy(t);

    return 0;
}
