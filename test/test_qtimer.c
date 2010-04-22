#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

int main(int argc, char *argv[])
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
    iprintf("time to find self and assert it: %g secs\n", qtimer_secs(t));

    qtimer_start(t);
    qtimer_stop(t);
    assert(qtimer_secs(t) >= 0.0);
    if (qtimer_secs(t) == 0.0) {
	iprintf("inlining reduces calltime to zero (apparently)\n");
    } else {
	iprintf("smallest measurable time: %g secs\n", qtimer_secs(t));
    }

    qtimer_destroy(t);

    return 0;
}
