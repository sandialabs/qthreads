#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static unsigned int target = 1000;
static aligned_t x = 0;

//pthread_mutex_t alldone = PTHREAD_MUTEX_INITIALIZER;

static aligned_t alldone;

static aligned_t thread(qthread_t * t, void *arg)
{
    int me = qthread_id(qthread_self());

    //printf("thread(%p): me %i\n", (void*) t, me);
    int foo = qthread_stackleft(t);
    //printf("%i bytes left\n", foo);

    assert(qthread_lock(t, &x) == 0);
    //printf("thread(%i): x=%d\n", me, x);
    x++;
    if (x == target)
	qthread_unlock(t, &alldone);
    assert(qthread_unlock(t, &x) == 0);
    return foo + me; /* to force them to be used */
}

int main(int argc, char *argv[])
{
    long int i;

    assert(qthread_initialize() == 0);

    NUMARG(target,"TEST_TARGET");
    CHECK_INTERACTIVE();

    qthread_lock(qthread_self(), &alldone);

    for (i = 0; i < target; i++) {
	int res = qthread_fork(thread, NULL, NULL);
	if (res != 0) {
	    printf("res = %i\n", res);
	}
	assert(res == 0);
    }

    qthread_lock(qthread_self(), &alldone);


    iprintf("Final value of x=%lu\n", (unsigned long)x);

    if (x == target)
	return 0;
    else
	return -1;
}
