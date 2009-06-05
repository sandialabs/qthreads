#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread/wavefront.h>
#include <qtimer.h>

static size_t ASIZE = 10;

void sum(const void *restrict left, const void *restrict leftdown,
	 const void *restrict down, void *restrict out)
{
    *(int *)out = *(int *)left + *(int *)leftdown + *(int *)down;
}

void suma(const void *restrict left, const void *restrict leftdown,
	  const void *restrict down, void *restrict out)
{
    *(aligned_t *) out =
	*(aligned_t *) left + *(aligned_t *) leftdown + *(aligned_t *) down;
}

void assign1(qthread_t * me, const size_t startat, const size_t stopat,
	     qarray * a, void *arg)
{
    aligned_t *ptr = qarray_elem_nomigrate(a, startat);
    const size_t max = stopat - startat;

    for (size_t i = 0; i < max; i++) {
	ptr[i] = 1;
    }
}

int main(int argc, char *argv[])
{
    int threads = 0;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	} else {
	    printf("%i threads\n", threads);
	}
    }
    if (argc >= 3) {
	ASIZE = strtol(argv[2], NULL, 0);
	printf("ASIZE: %i\n", ASIZE);
    }
    qthread_init(threads);
    {
	qthread_t *me = qthread_self();
	qarray **R = calloc(ASIZE, sizeof(qarray *));
	qtimer_t timer = qtimer_new();

	assert(R);
	assert(timer);
	R[0] = qarray_create_configured(ASIZE, sizeof(aligned_t), FIXED_HASH, 1, 1);
	qarray_iter_loop(me, R[0], 0, ASIZE, assign1, NULL);
	for (int col = 1; col < ASIZE; col++) {
	    R[col] = qarray_create_configured(ASIZE, sizeof(aligned_t), FIXED_HASH, 1, 1);
	    {
		aligned_t *ptr = qarray_elem_nomigrate(R[col], 0);

		*ptr = 1;
	    }
	}
	/* do stuff */
	qtimer_start(timer);
	for (int i = 0; i < 10; i++) {
	    qt_wavefront(R, ASIZE, sum);
	}
	qtimer_stop(timer);

	/* prove it */
	printf("wavefront secs: %f\n", qtimer_secs(timer) / 10.0);
	/* free it */
	for (int col = 0; col < ASIZE; col++) {
	    qarray_destroy(R[col]);
	}
	free(R);
	qtimer_free(timer);
    }
}
