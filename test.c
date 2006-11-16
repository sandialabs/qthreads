#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static int x = 0;
static int id = 0;
static int up[4] = { 1, 0, 0, 0 };
static int id2 = 0;

void thread(qthread_t * t)
{
    int me = 0;
    int next = 0;
    int i;

    qthread_lock(t, &id);
    me = id++;
    qthread_unlock(t, &id);
    next = (me + 1) % 4;
    /*printf("thread(%p): me %i next %i\n", t, me, next);*/

    for (i = 0; i < 1000; i++) {
	/* is it my turn? */
	qthread_lock(t, &(up[me]));

	qthread_lock(t, &x);
	/*printf("thread(%i): x=%d\n", me, x);*/
	x++;
	qthread_unlock(t, &x);
	/* set up the next guy's turn */
	qthread_unlock(t, &(up[next]));
    }
}

void thread2(qthread_t * t)
{
    int me = 0;
    int next = 0;
    int i;

    qthread_lock(t, &id2);
    me = id2++;
    qthread_unlock(t, &id2);
    next = (me + 1) % 4;
    /*printf("thread(%p): me %i next %i\n", t, me, next);*/

    for (i = 0; i < 1000; i++) {
	/* is it my turn? */
	qthread_lock(t, &(up[me]));

	qthread_lock(t, &x);
	/*printf("thread(%i): x=%d\n", me, x);*/
	x++;
	qthread_unlock(t, &x);
	/* set up the next guy's turn */
	qthread_unlock(t, &(up[next]));
    }
}

int main(int argc, char *argv[])
{
    qthread_t *a, *b, *c, *d;
    qthread_t *a2, *b2, *c2, *d2;

    qthread_init(3);

    qthread_lock(NULL, &(up[1]));
    qthread_lock(NULL, &(up[2]));
    qthread_lock(NULL, &(up[3]));

    a = qthread_fork(thread, NULL);
    b = qthread_fork(thread, NULL);
    c = qthread_fork(thread, NULL);
    d = qthread_fork(thread, NULL);
    a2 = qthread_fork(thread2, NULL);
    b2 = qthread_fork(thread2, NULL);
    c2 = qthread_fork(thread2, NULL);
    d2 = qthread_fork(thread2, NULL);

    qthread_join(NULL, a);
    qthread_join(NULL, b);
    qthread_join(NULL, c);
    qthread_join(NULL, d);
    qthread_join(NULL, a2);
    qthread_join(NULL, b2);
    qthread_join(NULL, c2);
    qthread_join(NULL, d2);

    qthread_finalize();

    if (x == 8000) {
	return 0;
    } else {
	fprintf(stderr, "Final value of x=%d\n", x);
	return -1;
    }
}
