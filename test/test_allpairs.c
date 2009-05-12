#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/allpairs.h>

size_t ASIZE = 1026;

aligned_t hamming = 0;

static void assignrand(qthread_t *me, const size_t startat, const size_t stopat, qarray * q, void *arg)
{
    int *ptr = qarray_elem_nomigrate(q, startat);

    for (size_t i = 0; i < (stopat-startat); i++) {
	ptr[i] = random()%2;
    }
}

static void assignrand2(qthread_t *me, const size_t startat, const size_t stopat, qarray * q, void *arg)
{
    int *ptr = qarray_elem_nomigrate(q, startat);

    for (size_t i = 0; i < (stopat-startat); i++) {
	ptr[i] = random()%2;
    }
}

static void printout(int *restrict * restrict out)
{
    size_t i;

    for (i = 0; i < ASIZE; i++) {
	size_t j;

	for (j = 0; j < ASIZE; j++) {
	    if (out[i][j] == -1) {
		printf("       _ ");
	    } else {
		printf("%8i ", out[i][j]);
	    }
	    assert(out[i][j] == out[j][i]);
	}
	printf("\n");
    }
}

static void mult(const int *inta, const int *intb, int *restrict out)
{
    assert(*out == -1);
    *out = (*inta) * (*intb);
}

static void hammingdist(const int *inta, const int * intb)
{
    if (*inta != *intb) {
	qthread_incr(&hamming, 1);
    }
}

int main(int argc, char *argv[])
{
    qarray *a1, *a2;
    int **out;
    size_t i;
    int threads = 0;
    int interactive = 0;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	    interactive = 0;
	} else {
	    interactive = 1;
	}
    }
    if (argc >= 3) {
	ASIZE = strtol(argv[2], NULL, 0);
    }

    qthread_init(threads);
    a1 = qarray_create_tight(ASIZE, sizeof(int));
    a2 = qarray_create_tight(ASIZE, sizeof(int));
    for (i = 0; i < ASIZE; i++) {
	*(int *)qarray_elem_nomigrate(a1, i) = i;
	*(int *)qarray_elem_nomigrate(a2, i) = i;
    }
    out = calloc(ASIZE, sizeof(int *));
    assert(out);
    for (i = 0; i < ASIZE; i++) {
	size_t j;
	out[i] = calloc(sizeof(int), ASIZE);
	assert(out[i]);
	for (j = 0; j < ASIZE; j++) {
	    out[i][j] = -1;
	}
    }

    qt_allpairs_output(a1, a2, (dist_out_f) mult, (void **)out, sizeof(int));
    if (interactive) {
	printout(out);
    }
    for (i = 0; i < ASIZE; i++) {
	free(out[i]);
    }
    free(out);

    /* trial #2 */
    qarray_iter_loop(qthread_self(), a1, 0, ASIZE, assignrand, NULL);
    qarray_iter_loop(qthread_self(), a2, 0, ASIZE, assignrand2, NULL);

    qt_allpairs(a1, a2, (dist_f)hammingdist);

    if (interactive) {
	printf("hamming sum = %lu\n", (unsigned long)hamming);
    }
    assert(hamming > 0);

    qarray_destroy(a1);
    qarray_destroy(a2);

    qthread_finalize();
    return 0;
}
