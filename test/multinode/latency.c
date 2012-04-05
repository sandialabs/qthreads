#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/multinode.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static aligned_t ping(void *arg)
{
    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret = 0;
    size_t count = 1;
    size_t payload_size = 0;
    aligned_t *payload = NULL;

    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, ping) == 0);
    assert(qthread_multinode_run() == 0);

    int const size = qthread_multinode_size();
    if (size < 2) {
        iprintf("Need more than one locale.\n");
        return 1;
    }

    NUMARG(count, "COUNT");
    NUMARG(payload_size, "SIZE");
    if (payload_size > 0) {
        payload = calloc(payload_size, sizeof(aligned_t));
        assert(payload);
    }

    qtimer_t timer = qtimer_create();

    // Round trip for a synchronized remote fork
    {
        qtimer_start(timer);
        for (size_t i = 0; i < count; i++) {
            qthread_fork_remote(ping, payload, &ret, 1, payload_size);
            qthread_readFF(NULL, &ret);
        }
        qtimer_stop(timer);

        double avg_time = qtimer_secs(timer) / count;
        iprintf("avg-spawn-sync: %f\n", avg_time);
    }

    // Spawn for asynchronous remote fork
    {
        aligned_t *rets = calloc(count, sizeof(aligned_t));

        qtimer_start(timer);
        for (size_t i = 0; i < count; i++) {
            qthread_fork_remote(ping, payload, &rets[i], 1, payload_size);
        }
        qtimer_stop(timer);
        for (size_t i = 0; i < count; i++) {
            qthread_readFF(NULL, &rets[i]);
        }

        double avg_time = qtimer_secs(timer) / count;
        iprintf("avg-spawn-async: %f\n", avg_time);

    }

    return 0;
}

