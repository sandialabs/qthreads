#include <stdlib.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread/wavefront.h>

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
    int interactive = 0;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	} else {
	    interactive = 1;
	}
    }
    if (argc >= 3) {
	ASIZE = strtol(argv[2], NULL, 0);
    }
    qthread_init(threads);
    {
	int **R = calloc(ASIZE, sizeof(int *));
	int i;

	for (i = 0; i < ASIZE; i++) {
	    R[i] = calloc(ASIZE, sizeof(int));
	    R[i][0] = 1;
	    if (i == 0) {
		for (i = 0; i < ASIZE; i++) {
		    R[0][i] = 1;
		}
		i = 0;
	    }
	}
	/* do stuff */
	qt_basic_wavefront(R, ASIZE, ASIZE, sum);

	/* prove it */
	if (interactive) {
	    for (int row = (ASIZE - 1); row >= 0; row--) {
		for (int col = 0; col < ASIZE; col++) {
		    printf("%7i ", R[col][row]);
		}
		printf("\n");
	    }
	    printf("\n");
	}

	/* free it */
	for (i = 0; i < ASIZE; i++) {
	    free(R[i]);
	}
	free(R);
    }
    {
	qthread_t *me = qthread_self();
	qarray **R = calloc(ASIZE, sizeof(qarray *));

	R[0] = qarray_create_tight(ASIZE, sizeof(aligned_t));
	qarray_iter_loop(me, R[0], 0, ASIZE, assign1, NULL);
	for (int col = 1; col < ASIZE; col++) {
	    R[col] = qarray_create_tight(ASIZE, sizeof(aligned_t));
	    {
		aligned_t *ptr = qarray_elem_nomigrate(R[col], 0);

		*ptr = 1;
	    }
	}
	/* do stuff */
	qt_wavefront(R, ASIZE, sum);

	/* prove it */
	if (interactive) {
	    for (int row = (ASIZE - 1); row >= 0; row--) {
		for (int col = 0; col < ASIZE; col++) {
		    aligned_t *tmpint = qarray_elem_nomigrate(R[col], row);

		    printf("%10u ", (unsigned int)*tmpint);
		}
		printf("\n");
	    }
	}
	/* free it */
	for (int col = 0; col < ASIZE; col++) {
	    qarray_destroy(R[col]);
	}
	free(R);
    }
}
