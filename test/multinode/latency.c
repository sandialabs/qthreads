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

static aligned_t intra_ping(void *arg)
{
    int shep = (int)qthread_shep();
    int next = (shep+1 == size) ? 0 : shep+1;

    iprintf("Ping shep %03d\n", shep);

    if (shep != 0) {
        qthread_fork_copyargs_to(intra_ping, payload, payload_size, NULL, next);
    } else if (count != 0) {
        count -= 1;
        qthread_fork_copyargs_to(intra_ping, payload, payload_size, NULL, 1);
    } else {
        qtimer_stop(timer);
        qthread_writeF_const(&done, 1);
    }

    return 0;
}

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

static aligned_t intra_node_test(void *arg)
{
    qthread_empty(&done);
    qtimer_start(timer);
    qthread_fork_copyargs_to(intra_ping, payload, payload_size, NULL, 1);
    qthread_readFF(NULL, &done);

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
        printf("Need more than one locale. Skipping test.\n");
        return 0;
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
    fprintf(stderr, "tot-time %f\n", total_time);
    fprintf(stderr, "avg-time %f\n", total_time / total_count);

    if (qthread_num_shepherds() > 1) {
        // Run intra-node test
        count = total_count / size;
        int size = (int)qthread_num_shepherds();
        total_count = count * size;

        aligned_t ret;
        qthread_fork_to(intra_node_test, NULL, &ret, 0);
        qthread_readFF(NULL, &ret);

        total_time = qtimer_secs(timer);
        fprintf(stderr, "intra-tot-time %f\n", total_time);
        fprintf(stderr, "intra-avg-time %f\n", total_time / total_count);
    }

    return 0;
}

