#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>
#include "qtimer.h"
#include "argparsing.h"

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

/* we can do this because we forced qarray to create tight segments */
void assign1_loop(qthread_t * me, const size_t startat, const size_t stopat,
		  qarray * qa, void *arg)
{
    double *ptr = qarray_elem_nomigrate(qa, startat);
    const size_t max = stopat - startat;

    for (size_t i = 0; i < max; i++) {
	ptr[i] = 1.0;
    }
}

void assert1_loop(qthread_t * me, const size_t startat, const size_t stopat,
		  qarray * qa, void *arg)
{
    const double *ptr = qarray_elem_nomigrate(qa, startat);
    size_t max = stopat - startat;

    for (size_t i = 0; i < max; i++) {
	assert(ptr[i] == 1.0);
    }
}

aligned_t assignall1(qthread_t * me, void *arg)
{
    memset(arg, 1, sizeof(bigobj));
    return 0;
}

void assignall1_loop(qthread_t * me, const size_t startat,
		     const size_t stopat, qarray * qa, void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate(qa, i);

	memset(ptr, 1, sizeof(bigobj));
    }
}

void assertall1_loop(qthread_t * me, const size_t startat,
		     const size_t stopat, qarray * qa, void *arg)
{
    bigobj *example = malloc(sizeof(bigobj));

    memset(example, 1, sizeof(bigobj));
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate(qa, i);

	assert(memcmp(ptr, example, sizeof(bigobj)) == 0);
    }
    free(example);
}

void assignoff1(qthread_t * me, const size_t startat, const size_t stopat,
		qarray * qa, void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate(qa, i);

	memset(ptr, 1, sizeof(offsize));
    }
}

void assertoff1(qthread_t * me, const size_t startat, const size_t stopat,
		qarray * qa, void *arg)
{
    offsize *example = malloc(sizeof(offsize));

    memset(example, 1, sizeof(offsize));
    for (size_t i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate(qa, i);

	assert(memcmp(ptr, example, sizeof(offsize)) == 0);
    }
    free(example);
}

int main(int argc, char *argv[])
{
    qarray *a;
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
    unsigned int dt_index;
    int enabled_tests = 7;
    int enabled_types = 255;

    assert(qthread_initialize() == QTHREAD_SUCCESS);

    verbose = 1;
    NUMARG(ELEMENT_COUNT, "TEST_ELEMENT_COUNT");
    NUMARG(enabled_tests, "TEST_ENABLED_TESTS");
    NUMARG(enabled_types, "TEST_ENABLED_TYPES");

    me = qthread_self();

    printf("Using %i shepherds\n", qthread_num_shepherds());
    printf("Arrays of %lu objects...\n", (unsigned long)ELEMENT_COUNT);

    if (enabled_types & 0x1) {
	printf("SERIAL:\n");
	if (enabled_tests & 1) {
	    size_t i, j;
	    double acctime = 0.0;
	    double *a = calloc(ELEMENT_COUNT, sizeof(double));

	    assert(a != NULL);

	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    a[i] = 1.0;
		}
		qtimer_stop(timer);
		acctime += qtimer_secs(timer);
	    }
	    printf("\tIteration over doubles: %f/", acctime / ITERATIONS);
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

	    assert(a != NULL);
	    assert(b != NULL);
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
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    assert(memcmp(&a[i], b, sizeof(bigobj)) == 0);
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    free(a);
	    free(b);
	    printf("%f secs\n", acc / ITERATIONS);
	}
	if (enabled_tests & 4) {
	    offsize *a = calloc(ELEMENT_COUNT, sizeof(offsize));
	    offsize *b = malloc(sizeof(offsize));
	    size_t i, j;
	    double acc = 0.0;

	    assert(a != NULL);
	    assert(b != NULL);
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
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		for (i = 0; i < ELEMENT_COUNT; i++) {
		    assert(memcmp(&a[i], b, sizeof(offsize)) == 0);
		}
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    free(a);
	    free(b);
	}
    }

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	if ((enabled_types & 1 << (dt_index + 1)) == 0)
	    continue;
	printf("%s:\n", distnames[dt_index]);
	/* test a basic array of doubles */
	if (enabled_tests & 1) {
	    int j;
	    double acc = 0.0;
	    a = qarray_create_configured(ELEMENT_COUNT, sizeof(double),
					 disttypes[dt_index], 1, 0);
	    assert(a != NULL);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assign1_loop, NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over doubles: %f/", acc / ITERATIONS);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assert1_loop, NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    qarray_destroy(a);
	}

	/* now test an array of giant things */
	if (enabled_tests & 2) {
	    int j;
	    double acc = 0.0;

	    a = qarray_create_configured(ELEMENT_COUNT, sizeof(bigobj),
					 disttypes[dt_index], 0, 0);
	    assert(a != NULL);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assignall1_loop,
				 NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over giants: %f/", acc / ITERATIONS);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assertall1_loop,
				 NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    qarray_destroy(a);
	}

	/* now test an array of weird-sized things */
	if (enabled_tests & 4) {
	    int j;
	    double acc = 0.0;

	    a = qarray_create_configured(ELEMENT_COUNT, sizeof(offsize),
					 disttypes[dt_index], 0, 0);
	    assert(a != NULL);
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assignoff1, NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("\tIteration over weirds: %f/", acc / ITERATIONS);
	    acc = 0.0;
	    for (j = 0; j < ITERATIONS; j++) {
		qtimer_start(timer);
		qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assertoff1, NULL);
		qtimer_stop(timer);
		acc += qtimer_secs(timer);
	    }
	    printf("%f secs\n", acc / ITERATIONS);
	    qarray_destroy(a);
	}
    }

    return 0;
}
