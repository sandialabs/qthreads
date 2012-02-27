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

static aligned_t say_hello(void *arg)
{
    int const rank = qthread_multinode_rank();

    iprintf("Hello from locale %03d!\n", rank);

    return 0;
}

int main(int argc, char *argv[])
{
    CHECK_VERBOSE();

    setenv("QT_MULTINODE", "yes", 1);
    assert(qthread_initialize() == 0);
    assert(qthread_multinode_register(2, say_hello) == 0);
    assert(qthread_multinode_run() == 0);

    int const size = qthread_multinode_size();
    aligned_t rets[size];

    for (int i = 0; i < size; i++)
        qthread_fork_remote(say_hello, NULL, &rets[i], i, 0);
    for (int i = 0; i < size; i++)
        qthread_readFF(NULL, &rets[i]);

    return 0;
}

