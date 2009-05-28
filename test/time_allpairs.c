#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/allpairs.h>
#include <qtimer.h>

size_t ASIZE = 1026;

aligned_t hamming = (aligned_t) - 1;

static void assigni(qthread_t * me, const size_t startat, const size_t stopat,
		    qarray * q, void *arg)
{
    int *ptr = qarray_elem_nomigrate(q, startat);

    for (size_t i = 0; i < (stopat - startat); i++) {
	ptr[i] = (i + startat);
    }
}

static void assignrand(qthread_t * me, const size_t startat,
		       const size_t stopat, qarray * q, void *arg)
{
    int *ptr = qarray_elem_nomigrate(q, startat);

    for (size_t i = 0; i < (stopat - startat); i++) {
	ptr[i] = random();
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
    *out = (*inta) * (*intb);
}

static void hammingdist(const int *inta, const int *intb)
{
    unsigned int ham = *inta ^ *intb;
    aligned_t hamdist = 0;
    qthread_t *me = qthread_self();

    while (ham != 0) {
	hamdist += ham & 1;
	ham >>= 1;
    }
    if (hamming > hamdist) {
	qthread_lock(me, &hamming);
	if (hamming > hamdist) {
	    hamming = hamdist;
	}
	qthread_unlock(me, &hamming);
    }
}

int main(int argc, char *argv[])
{
    qarray *a1, *a2;
    int **out;
    size_t i;
    int threads = 0;
    int interactive = 0;
    qthread_t *me;
    qtimer_t timer = qtimer_new();

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	    interactive = 0;
	} else {
	    printf("threads: %i\n", threads);
	    interactive = 1;
	}
    }
    if (argc >= 3) {
	ASIZE = strtol(argv[2], NULL, 0);
	printf("ASIZE: %i\n", (int)ASIZE);
    }

    if (qthread_init(threads) != QTHREAD_SUCCESS) {
	fprintf(stderr, "qthread library could not be initialized!\n");
	exit(EXIT_FAILURE);
    }
    me = qthread_self();

    a1 = qarray_create_configured(ASIZE, sizeof(int), FIXED_HASH, 1, 1);
    a2 = qarray_create_configured(ASIZE, sizeof(int), FIXED_HASH, 1, 1);
    printf("segments of %u elements\n", (unsigned int)a1->segment_size);
    qarray_iter_loop(me, a1, 0, ASIZE, assigni, NULL);
    qarray_iter_loop(me, a2, 0, ASIZE, assigni, NULL);

    out = calloc(ASIZE, sizeof(int *));
    assert(out);
    for (i = 0; i < ASIZE; i++) {
	size_t j;
	out[i] = calloc(sizeof(int), ASIZE);
	assert(out[i]);
    }
    printf("all initialized\n");

    qtimer_start(timer);
    for (int i=0; i<10; i++) {
	qt_allpairs_output(a1, a2, (dist_out_f) mult, (void **)out, sizeof(int));
    }
    qtimer_stop(timer);
    printf("mult time: %f\n", qtimer_secs(timer)/10.0);
    for (i = 0; i < ASIZE; i++) {
	free(out[i]);
    }
    free(out);

    qtimer_free(timer);

    qarray_destroy(a1);
    qarray_destroy(a2);

    qthread_finalize();
    return 0;
}
