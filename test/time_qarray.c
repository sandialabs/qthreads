#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>
#include "qtimer.h"

#define ITERATIONS 10
size_t ELEMENT_COUNT = 100000;

typedef struct
{
    char pad[10000];
} bigobj;
typedef struct
{
    char pad[40];
} offsize;

aligned_t assign1(qthread_t * me, void *arg)
{
    *(double *)arg = 1.0;
    return 0;
}

void assign1_loop(qthread_t * me, const size_t startat, const size_t stopat,
		  void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	double *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	*ptr = 1.0;
    }
}

void assert1_loop(qthread_t * me, const size_t startat, const size_t stopat,
		  void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	double *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	assert(*ptr == 1.0);
    }
}

aligned_t assignall1(qthread_t * me, void *arg)
{
    memset(arg, 1, sizeof(bigobj));
    return 0;
}

void assignall1_loop(qthread_t * me, const size_t startat,
		     const size_t stopat, void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	memset(ptr, 1, sizeof(bigobj));
    }
}

void assertall1_loop(qthread_t * me, const size_t startat,
		     const size_t stopat, void *arg)
{
    bigobj *example = malloc(sizeof(bigobj));

    memset(example, 1, sizeof(bigobj));
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	assert(memcmp(ptr, example, sizeof(bigobj)) == 0);
    }
    free(example);
}

void assignoff1(qthread_t * me, const size_t startat, const size_t stopat,
		void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	memset(ptr, 1, sizeof(offsize));
    }
}

void assertoff1(qthread_t * me, const size_t startat, const size_t stopat,
		void *arg)
{
    offsize *example = malloc(sizeof(offsize));

    memset(example, 1, sizeof(offsize));
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate((qarray *) arg, i);

	assert(memcmp(ptr, example, sizeof(offsize)) == 0);
    }
    free(example);
}

int main(int argc, char *argv[])
{
    qarray *a;
    int threads = 1;
    qthread_t *me;
    qtimer_t timer = qtimer_new();
    distribution_t disttypes[] = {
	FIXED_HASH, ALL_LOCAL, /*ALL_RAND, ALL_LEAST, */ DIST_RAND,
	DIST_REG_STRIPES, DIST_REG_FIELDS, DIST_LEAST
    };
    const char *distnames[] = {
	"FIXED_HASH", "ALL_LOCAL", /*"ALL_RAND", "ALL_LEAST", */ "DIST_RAND",
	"DIST_REG_STRIPES", "DIST_REG_FIELDS", "DIST_LEAST", "SERIAL"
    };
    int dt_index;
    int interactive = 0;
    int enabled_tests = 7;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads <= 0) {
	    threads = 1;
	    interactive = 0;
	} else {
	    interactive = 1;
	}
    }
    if (argc >= 3) {
	ELEMENT_COUNT = strtol(argv[2], NULL, 0);
    }
    if (argc >= 4) {
	enabled_tests = strtol(argv[3], NULL, 0);
    }
    if (!interactive) {
	return 0;
    }

    qthread_init(threads);
    me = qthread_self();

    printf("Using %i shepherds\n", threads);
    printf("Arrays of %lu objects...\n", (unsigned long)ELEMENT_COUNT);

    printf("SERIAL:\n");
    {
	const size_t last_type = (sizeof(disttypes) / sizeof(distribution_t));

	if (enabled_tests & 1) {
	    size_t i, j;
	    double acctime = 0.0;
	    double *a = calloc(ELEMENT_COUNT, sizeof(double));

	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    a[i] = 1.0;
		}
		qtimer_stop(timer);
		acctime += qtimer_secs(timer);
	    }
	    printf("\tIteration over doubles: %f/", acctime / ITERATIONS);
	    fflush(stdout);
	    acctime = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    assert(a[i] == 1.0);
		}
		qtimer_stop(timer);
		acctime += qtimer_secs(timer);
	    }
	    free(a);
	    printf("%f secs\n", acctime / ITERATIONS);
	}
	if (enabled_tests & 2) {
	    bigobj *a = calloc(ELEMENT_COUNT, sizeof(bigobj));
	    bigobj *b = malloc(sizeof(bigobj));
	    size_t i, j;
	    double acc = 0.0;

	    memset(b, 1, sizeof(bigobj));
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    memset(&a[i], 1, sizeof(bigobj));
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over giants: %f/", acc / ITERATIONS);
	    fflush(stdout);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    assert(memcmp(a, b, sizeof(bigobj)) == 0);
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    free(a);
	    free(b);
	    printf("%f secs\n", acc / ITERATIONS);
	    fflush(stdout);
	}
	if (enabled_tests & 4) {
	    offsize *a = calloc(ELEMENT_COUNT, sizeof(offsize));
	    offsize *b = malloc(sizeof(offsize));
	    size_t i, j;
	    double acc = 0.0;

	    memset(b, 1, sizeof(offsize));
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    memset(&a[i], 1, sizeof(offsize));
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over weirds: %f/", acc / ITERATIONS);
	    fflush(stdout);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    assert(memcmp(a, b, sizeof(offsize)) == 0);
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    fflush(stdout);
	    free(a);
	    free(b);
	}
    }

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	printf("%s:\n", distnames[dt_index]);
	/* test a basic array of doubles */
	if (enabled_tests & 1) {
	    int j;
	    double acc = 0.0;
	    a = qarray_create(ELEMENT_COUNT, sizeof(double),
			      disttypes[dt_index]);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assign1_loop);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over doubles: %f/", acc / ITERATIONS);
	    fflush(stdout);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assert1_loop);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    fflush(stdout);
	    qarray_free(a);
	}

	/* now test an array of giant things */
	if (enabled_tests & 2) {
	    int j;
	    double acc = 0.0;

	    a = qarray_create(ELEMENT_COUNT, sizeof(bigobj),
			      disttypes[dt_index]);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assignall1_loop);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over giants: %f/", acc / ITERATIONS);
	    fflush(stdout);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assertall1_loop);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    fflush(stdout);
	    qarray_free(a);
	}

	/* now test an array of weird-sized things */
	if (enabled_tests & 4) {
	    int j;
	    double acc = 0.0;

	    a = qarray_create(ELEMENT_COUNT, sizeof(offsize),
			      disttypes[dt_index]);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assignoff1);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over weirds: %f/", acc / ITERATIONS);
	    fflush(stdout);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, assertoff1);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    fflush(stdout);
	    qarray_free(a);
	}
    }

    qthread_finalize();
    return 0;
}
