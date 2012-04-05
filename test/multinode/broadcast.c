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

static int size;
static int rank;

static aligned_t pong(void *arg)
{
    qthread_incr(&donecount, 1);

    return 0;
}

static aligned_t ping(void *arg)
{
    qthread_fork_remote(pong, NULL, NULL, 0, 0);

    return 0;
}

int main(int argc, char *argv[])
{
    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, ping) == 0);
    assert(qthread_multinode_register(3, pong) == 0);

    size = qthread_multinode_size();
    rank = qthread_multinode_rank();
    iprintf("rank %d size %d\n", rank, size);

    assert(qthread_multinode_run() == 0);

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

    for (int i = 0; i < count; i++) {
        iprintf("Spawned task %d\n", i);
        qthread_fork_remote(ping, payload, NULL, (i % size), payload_size);
    }
    iprintf("Waiting ...\n");
    while (donecount < count) {
        qthread_yield();
    }
    iprintf("Done.\n");

    return 0;
}

