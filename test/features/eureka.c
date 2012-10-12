#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "argparsing.h"
#include "qthread_innards.h"
#include "qt_shepherd_innards.h"

static aligned_t t  = 1;
static aligned_t t2 = 1;
static aligned_t t3 = 1;

static void alive_check(const size_t a, const size_t b, void *junk)
{
    return;
}

static aligned_t waiter(void *arg)
{
    iprintf("waiter alive!\n");
    qthread_yield();
    while(t == 1) {
	COMPILER_FENCE;
    }
    iprintf("waiter exiting!\n");

    return 0;
}

static aligned_t parent(void *arg)
{
    iprintf("parent alive!\n");
    qthread_fork(waiter, NULL, &t3);
    qthread_fork(waiter, NULL, &t3);
    qthread_yield();
    iprintf("parent about to eureka...\n");
    qt_team_eureka();
    iprintf("parent still alive!\n");
    COMPILER_FENCE;
    t = 0;
    return 0;
}

int main(int   argc,
         char *argv[])
{
    int ret = 0;

    ret = qthread_init(3);
    if (ret != QTHREAD_SUCCESS) {
	fprintf(stderr, "initialization error\n");
	abort();
    }

    CHECK_VERBOSE();

    iprintf("%i shepherds...\n", qthread_num_shepherds());
    iprintf("  %i threads total\n", qthread_num_workers());

    qt_loop_balance(0, qthread_num_workers(), alive_check, NULL);

    qthread_fork_new_team(parent, NULL, &t2);

    qthread_readFF(NULL, &t2);

    iprintf("Success!\n");

    return 0;
}

/* vim:set expandtab */
