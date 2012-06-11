#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/multinode.h>
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

    assert(qthread_multinode_multistart() == 0);

    int const here = qthread_multinode_rank();
    int const there = (here+1 == qthread_multinode_size()) ? 0 : here+1;

    iprintf("[%03d] Single Program Multiple Hello!\n", here);

    aligned_t ret;
    qthread_fork_remote(say_hello, NULL, &ret, there, 0);
    qthread_readFF(NULL, &ret);

    assert(qthread_multinode_multistop() == 0);

    return 0;
}

