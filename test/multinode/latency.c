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
static size_t count = 1;
static qtimer_t timer;

static int size;
static int rank;
static int next;
static aligned_t done;

static aligned_t ping(void *arg)
{
    iprintf("Ping %03d\n", rank);

    if (rank != 0) {
        qthread_fork_remote(ping, payload, NULL, next, payload_size);
    } else if (count != 0) {
        count -= 1;
        qthread_fork_remote(ping, payload, NULL, 1, payload_size);
    } else {
        qtimer_stop(timer);
        qthread_writeF_const(&done, 1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, ping) == 0);

    size = qthread_multinode_size();
    rank = qthread_multinode_rank();
    next = (rank+1 == size) ? 0 : rank+1;
    iprintf("rank %d next %d size %d\n", rank, next, size);

    assert(qthread_multinode_run() == 0);

    if (size < 2) {
        iprintf("Need more than one locale.\n");
        return 1;
    }

    NUMARG(count, "COUNT");
    size_t total_count = count * size;
    count--;

    NUMARG(payload_size, "SIZE");
    if (payload_size > 0) {
        payload = calloc(payload_size, sizeof(aligned_t));
        assert(payload);
    }

    timer = qtimer_create();

    qthread_empty(&done);
    qtimer_start(timer);
    qthread_fork_remote(ping, payload, NULL, 1, payload_size);
    qthread_readFF(NULL, &done);

    double total_time = qtimer_secs(timer);
    iprintf("tot-time %f\n", total_time);
    iprintf("avg-time %f\n", total_time / total_count);

    return 0;
}

