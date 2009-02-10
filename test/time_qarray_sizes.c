#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>
#include "qtimer.h"

size_t ELEMENT_COUNT = 100000;
size_t constant_size = 1073741824/*1GB*/; /* 536870912 = 512MB, 104857600 = 100MB */
size_t global_size = 0;

void assign1_loop(qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t size = global_size;
    for (size_t i = startat; i < stopat; i++) {
	memset(((char *)arg)+(size * i), 1, size);
    }
}

void assert1_loop(qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t size = global_size;
    char * example = malloc(size);

    memset(example, 1, size);
    for (size_t i = startat; i < stopat; i++) {
	if (memcmp(((char *)arg)+(size * i), example, size) != 0) {
	    printf("...assignment failed! (%lu bytes)\n", (unsigned long)size);
	    assert(0);
	}
    }
}

int main(int argc, char *argv[])
{
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
    const size_t sizes[] =
	{ 4, 8, 11, 16, 23, 32, 64, 65, 71, 100, 128, 256, 301, 333,
512, 1024, 2048, 4096, 5000, 10000, 16384 };
    int dt_index;
    int interactive = 0;

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
	interactive = 1;
    }
    if (argc >= 4) {
	constant_size = strtol(argv[3], NULL, 0);
    }
    if (interactive == 0) {
	return 0;
    }

    qthread_init(threads);
    me = qthread_self();

    printf("Arrays of %lu objects...\n", (unsigned long)ELEMENT_COUNT);

    printf("SERIAL:\n");
    {
	size_t size_i;

	printf("\tSize, Assignment, Reading\n");
	for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
	    size_t i, size=sizes[size_i];
	    char *a = calloc(ELEMENT_COUNT, size);
	    char *example = malloc(size);

	    memset(example, 1, size);
	    qtimer_start(timer);
	    for (i=0; i<ELEMENT_COUNT; i++) {
		memset(a+(i*size), 1, size);
	    }
	    qtimer_stop(timer);
	    printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer));
	    fflush(stdout);
	    qtimer_start(timer);
	    for (i=0; i<ELEMENT_COUNT; i++) {
		if (memcmp(a+(i*size), example, size) != 0) {
		    printf("...assignment failed! (%lu bytes, %lu'th element)\n", (unsigned long)size, (unsigned long)i);
		    assert(0);
		}
	    }
	    qtimer_stop(timer);
	    printf(", %f\n", qtimer_secs(timer));
	    fflush(stdout);
	    free(a);
	    free(example);
	}
	printf("\tSize, Assignment, Reading\n");
	for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
	    size_t i, size=sizes[size_i];
	    char *example = malloc(size);
	    size_t count = constant_size/size;
	    char *a = calloc(count, size);

	    memset(example, 1, size);
	    qtimer_start(timer);
	    for (i=0; i<count; i++) {
		memset(a+(i*size), 1, size);
	    }
	    qtimer_stop(timer);
	    printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer));
	    fflush(stdout);
	    qtimer_start(timer);
	    for (i=0; i<count; i++) {
		if (memcmp(a+(i*size), example, size) != 0) {
		    printf("...assignment failed! (%lu bytes, %lu'th element)\n", (unsigned long)size, (unsigned long)i);
		    assert(0);
		}
	    }
	    qtimer_stop(timer);
	    printf(", %f\n", qtimer_secs(timer));
	    fflush(stdout);
	    free(a);
	    free(example);
	}
    }

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	printf("%s:\n", distnames[dt_index]);
	{
	    size_t size_i;

	    printf("\tSize, Assignment, Reading\n");
	    for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
		const size_t size=sizes[size_i];
		qarray *a = qarray_create(ELEMENT_COUNT, size, disttypes[dt_index]);

		global_size = size;
		qtimer_start(timer);
		qarray_iter_loop(me, a, assign1_loop);
		qtimer_stop(timer);
		printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer));
		fflush(stdout);
		qtimer_start(timer);
		qarray_iter_loop(me, a, assert1_loop);
		qtimer_stop(timer);
		printf(", %f\n", qtimer_secs(timer));
		fflush(stdout);
		qarray_free(a);
	    }
	    printf("\tSize, Assignment, Reading\n");
	    for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
		const size_t size=sizes[size_i];
		const size_t count = constant_size/size; /* 1GB */
		qarray *a = qarray_create(count, size, disttypes[dt_index]);

		global_size = size;
		qtimer_start(timer);
		qarray_iter_loop(me, a, assign1_loop);
		qtimer_stop(timer);
		printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer));
		fflush(stdout);
		qtimer_start(timer);
		qarray_iter_loop(me, a, assert1_loop);
		qtimer_stop(timer);
		results[size_i][1] = qtimer_secs(timer);
		printf(", %f\n", qtimer_secs(timer));
		fflush(stdout);
		qarray_free(a);
	    }
	}
    }

    qthread_finalize();
    return 0;
}
