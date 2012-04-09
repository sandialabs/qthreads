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

static size_t payload_size = 0;
static aligned_t *payload = NULL;
static aligned_t count = 1;
static aligned_t donecount = 0;
static int msgs = 1;

static int size;
static int rank;

static aligned_t pong(void *arg)
{
    iprintf("pong from %d\n", rank);
    qthread_incr(&donecount, 1);

    return 0;
}

static aligned_t ping(void *arg)
{
    iprintf("ping donecout %d msgs %d\n", donecount, msgs);
    if (msgs-1 == qthread_incr(&donecount, 1)) {
        iprintf("\tping donecout %d msgs %d\n", donecount, msgs);
        qthread_fork_remote(pong, NULL, NULL, 0, 0);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    qtimer_t timer;

    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, ping) == 0);
    assert(qthread_multinode_register(3, pong) == 0);

    size = qthread_multinode_size();
    rank = qthread_multinode_rank();
    iprintf("rank %d size %d\n", rank, size);

    NUMARG(count, "COUNT");

    msgs = count / (size-1);

    assert(qthread_multinode_run() == 0);

    if (size < 2) {
        iprintf("Need more than one locale.\n");
        return 1;
    }

    NUMARG(payload_size, "SIZE");
    if (payload_size > 0) {
        payload = calloc(payload_size, sizeof(aligned_t));
        assert(payload);
    }

    timer = qtimer_create();

    int next = 1;
    qtimer_start(timer);
    for (int i = 0; i < msgs*(size-1); i++) {
        iprintf("Spawned task %d to %d\n", i, next);
        qthread_fork_remote(ping, payload, NULL, next, payload_size);
        next = (next+1 == size) ? 1 : next+1;
    }
    iprintf("Waiting ...\n");
    while (donecount < size-1) {
        qthread_yield();
    }
    iprintf("Done.\n");
    qtimer_stop(timer);

    fprintf(stderr, "exec_time %f\n", qtimer_secs(timer));
    fprintf(stderr, "msg_rate %f\n", (msgs*(size-1))/qtimer_secs(timer));

    return 0;
}

