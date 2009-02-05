#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>
#include "qtimer.h"

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
	((double *)arg)[i] = 1.0;
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
	memset(((char *)arg) + (sizeof(bigobj) * i), 1, sizeof(bigobj));
    }
}

void assignoff1(qthread_t * me, const size_t startat, const size_t stopat,
		void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	memset(((char *)arg) + (sizeof(offsize) * i), 1, sizeof(offsize));
    }
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
    double results[(sizeof(disttypes) / sizeof(distribution_t)) + 1][3] = { {0}
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

    qthread_init(threads);
    me = qthread_self();

    printf("Arrays of %lu objects...\n", (unsigned long)ELEMENT_COUNT);

    printf("SERIAL:\n");
    {
	size_t i;
	const size_t last_type = (sizeof(disttypes) / sizeof(distribution_t));

	if (enabled_tests & 1) {
	    double *a = calloc(ELEMENT_COUNT, sizeof(double));

	    qtimer_start(timer);
	    for (i = 0; i < ELEMENT_COUNT; i++) {
		a[i] = 1.0;
	    }
	    qtimer_stop(timer);
	    for (i = 0; i < ELEMENT_COUNT; i++) {
		assert(a[i] == 1.0);
	    }
	    free(a);
	    results[last_type][0] = qtimer_secs(timer);
	    printf("\tIteration over doubles: %f secs\n",
		   results[last_type][0]);
	}
	if (enabled_tests & 2) {
	    bigobj *a = calloc(ELEMENT_COUNT, sizeof(bigobj));

	    qtimer_start(timer);
	    for (i = 0; i < ELEMENT_COUNT; i++) {
		memset(&a[i], 1, sizeof(bigobj));
	    }
	    qtimer_stop(timer);
	    free(a);
	    results[last_type][1] = qtimer_secs(timer);
	    printf("\tIteration over giants: %f secs\n",
		   results[last_type][1]);
	}
	if (enabled_tests & 4) {
	    offsize *a = calloc(ELEMENT_COUNT, sizeof(offsize));

	    qtimer_start(timer);
	    for (i = 0; i < ELEMENT_COUNT; i++) {
		memset(&a[i], 1, sizeof(offsize));
	    }
	    qtimer_stop(timer);
	    free(a);
	    results[last_type][2] = qtimer_secs(timer);
	    printf("\tIteration over weirds: %f secs\n",
		   results[last_type][2]);
	}
    }

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	printf("%s:\n", distnames[dt_index]);
	/* test a basic array of doubles */
	if (enabled_tests & 1) {
	    a = qarray_create(ELEMENT_COUNT, sizeof(double),
			      disttypes[dt_index]);
	    qtimer_start(timer);
	    qarray_iter_loop(me, a, assign1_loop);
	    qtimer_stop(timer);
	    results[dt_index][0] = qtimer_secs(timer);
	    printf("\tIteration over doubles: %f secs (loop)\n",
		   results[dt_index][0]);
	    qarray_free(a);
	}

	/* now test an array of giant things */
	if (enabled_tests & 2) {
	    a = qarray_create(ELEMENT_COUNT, sizeof(bigobj),
			      disttypes[dt_index]);
	    qtimer_start(timer);
	    qarray_iter_loop(me, a, assignall1_loop);
	    qtimer_stop(timer);
	    results[dt_index][1] = qtimer_secs(timer);
	    printf("\tIteration over giants: %f secs (loop)\n",
		   results[dt_index][1]);
	    qarray_free(a);
	}

	/* now test an array of weird-sized things */
	if (enabled_tests & 4) {
	    a = qarray_create(ELEMENT_COUNT, sizeof(offsize),
			      disttypes[dt_index]);
	    qtimer_start(timer);
	    qarray_iter_loop(me, a, assignoff1);
	    qtimer_stop(timer);
	    qarray_free(a);
	    results[dt_index][2] = qtimer_secs(timer);
	    printf("\tIteration over weirds: %f secs\n",
		   results[dt_index][2]);
	}
    }

    {
	double fastest_time, average_time;
	size_t fastest_type;
	const char *types[] = { "double", "giant", "weird" };
	int i;

	for (i = 0; i < 3; i++) {
	    if (enabled_tests & (1 << i)) {
		fastest_time = results[0][i];
		fastest_type = 0;
		average_time = results[0][i];
		for (dt_index = 1;
		     dt_index <
		     (sizeof(disttypes) / sizeof(distribution_t) + 1);
		     dt_index++) {
		    if (fastest_time > results[dt_index][i]) {
			fastest_time = results[dt_index][i];
			fastest_type = dt_index;
		    }
		    average_time += results[dt_index][i];
		}
		average_time /= (sizeof(disttypes) / sizeof(distribution_t) +
				 1);
		printf("Fastest %s iterator: %s (%f secs, avg %f secs)\n",
		       types[i], distnames[fastest_type], fastest_time,
		       average_time);
	    }
	}
    }

    qthread_finalize();
    return 0;
}
