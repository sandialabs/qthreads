#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/qthread_multinode.h>
#include "argparsing.h"

static aligned_t done;

static aligned_t ping(void *arg)
{
    int const size = qthread_multinode_size();
    int const rank = qthread_multinode_rank();

    iprintf("Ping %03d\n", rank);

    if (rank == 0) {
        qthread_writeF_const(&done, 1);
    } else {
        int const next = (rank + 1) % size;
        qthread_fork_remote(ping, NULL, NULL, next, 0);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, ping) == 0);
    assert(qthread_multinode_run() == 0);

    int const size = qthread_multinode_size();
    int const rank = qthread_multinode_rank();

    int const next = (rank + 1) % size;

    qthread_empty(&done);
    qthread_fork_remote(ping, NULL, NULL, next, 0);
    qthread_readFF(NULL, &done);

    return 0;
}

