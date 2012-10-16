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

aligned_t alive[2];

static void alive_check(const size_t a, const size_t b, void *junk)
{
    return;
}

static aligned_t waiter(void *arg)
{
    const int assigned = (int)(intptr_t)arg;
    const int id = qthread_id();
    iprintf("waiter %i alive! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));
    qthread_fill(&alive[assigned]);
    qthread_flushsc();
    while(t == 1) {
	COMPILER_FENCE;
    }
    iprintf("waiter %i exiting! id %i wkr %u\n", assigned, id, qthread_readstate(CURRENT_UNIQUE_WORKER));

    return 0;
}

static aligned_t parent(void *arg)
{
    iprintf("parent alive!\n");
    qthread_empty(alive+0);
    qthread_empty(alive+1);
    qthread_fork(waiter, (void*)(intptr_t)0, &t3);
    qthread_fork(waiter, (void*)(intptr_t)1, &t3);
    iprintf("parent waiting on %p\n", &alive[1]);
    qthread_readFF(NULL, &alive[1]);
    iprintf("saw waiter 1 report in\n");
    iprintf("parent waiting on %p\n", &alive[0]);
    qthread_readFF(NULL, &alive[0]);
    iprintf("saw waiter 0 report in\n");
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
