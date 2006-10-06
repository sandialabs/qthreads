#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static int x = 0;
static int id = 0;
static int up[4] = { 1, 0, 0, 0 };

void thread(qthread_t * t)
{
    int me = 0;
    int next = 0;
    int i;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);
    next = (me + 1) % 4;
    printf("thread(%p): me %i next %i\n", t, me, next);

    for (i = 0; i < 10; i++) {
	while (1) {
	    qthread_lock(t, &(up[me]));
	    if (up[me] != 0) {
		up[me] = 0;
		qthread_unlock(t, &(up[me]));
		break;
	    }
	    qthread_unlock(t, &(up[me]));
	}
	qthread_lock(t, &x);
	printf("thread(%i): x=%d\n", me, x);
	x++;
	qthread_unlock(t, &x);
	qthread_lock(t, &(up[next]));
	up[next]++;
	printf("{%i,%i,%i,%i}\n", up[0], up[1], up[2], up[3]);
	qthread_unlock(t, &(up[next]));
    }
}

int main(int argc, char *argv[])
{
    qthread_t *a, *b, *c, *d;

    qthread_init(1);

    a = qthread_fork(thread, NULL);
    b = qthread_fork(thread, NULL);
    c = qthread_fork(thread, NULL);
    d = qthread_fork(thread, NULL);

    qthread_join(a);
    qthread_join(b);
    qthread_join(c);
    qthread_join(d);

    qthread_finalize();

    printf("Final value of x=%d\n", x);
}
