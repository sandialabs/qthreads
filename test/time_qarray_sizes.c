#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>
#include "qtimer.h"

size_t ELEMENT_COUNT = 100000;
size_t constant_size = 1073741824/*1GB*/; /* 536870912 = 512MB, 104857600 = 100MB */
size_t global_size = 0;

void assign1_loop(qthread_t *me, const size_t startat, const size_t stopat, qarray *qa, void *arg)
{
    const size_t size = global_size;
    size_t i;
    const char shep = qthread_shep(me);

    for (i = startat; i < stopat; i++) {
	void *ptr = qarray_elem_nomigrate(qa, i);
	memset(ptr, shep, size);
    }
}

void assert1_loop(qthread_t *me, const size_t startat, const size_t stopat, qarray *qa, void *arg)
{
    const size_t size = global_size;
    char * example = malloc(size);
    size_t i;

    memset(example, qthread_shep(me), size);
    for (i = startat; i < stopat; i++) {
	char *ptr = qarray_elem_nomigrate(qa, i);
	assert(ptr != NULL);
	if (memcmp(ptr, example, size) != 0) {
	    printf("...assignment failed! (%lu bytes)\n", (unsigned long)size);
	    assert(0);
	}
    }
    free(example);
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
	{ 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 23, 32, 64, 65, 71, 100, 128, 256, 301, 333,
512/*, 1024, 2048, 4096, 5000, 10000, 16384*/ };
    int dt_index;
    unsigned dist_requested = 63;
    int interactive = 0;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 1;
	    interactive = 0;
	} else {
	    interactive = 1;
	}
    }
    if (argc >= 3) {
	dist_requested = strtoul(argv[2], NULL, 0);
    }
    if (argc >= 4) {
	ELEMENT_COUNT = strtoul(argv[3], NULL, 0);
    }
    if (argc >= 5) {
	constant_size = strtoul(argv[4], NULL, 0);
    }
    if (interactive == 0) {
	return 0;
    }

    qthread_init(threads);
    me = qthread_self();

    printf("Arrays of %lu objects...\n", (unsigned long)ELEMENT_COUNT);

#if 0
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
		char *ptr = a+(i*size);
		if (memcmp(ptr, example, size) != 0) {
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
	    size_t count = constant_size/size;
	    char *a = calloc(count, size);
	    char *example = malloc(size);

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
		char *ptr = a+(i*size);
		if (memcmp(ptr, example, size) != 0) {
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
#endif

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	if (((dist_requested >> dt_index) & 1) == 0) {
	    continue;
	}
	printf("%s:\n", distnames[dt_index]);
	{
	    size_t size_i;

	    printf("\tSize, Assignment, Reading\n");
	    for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
		const size_t size=sizes[size_i];
		qarray *a = qarray_create(ELEMENT_COUNT, size, disttypes[dt_index]);

		global_size = size;
		qtimer_start(timer);
		for (unsigned i=0;i<10;i++) {
		    qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assign1_loop, NULL);
		}
		qtimer_stop(timer);
		printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer)/10.0);
		fflush(stdout);
		qtimer_start(timer);
		for (unsigned i=0;i<10;i++) {
		    qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assert1_loop, NULL);
		}
		qtimer_stop(timer);
		printf(", %f\n", qtimer_secs(timer)/10.0);
		fflush(stdout);
		qarray_destroy(a);
	    }
	    if (constant_size == 0) continue;
	    printf("\tSize, Assignment, Reading\n");
	    for (size_i=0; size_i<(sizeof(sizes)/sizeof(size_t)); size_i++) {
		const size_t size=sizes[size_i];
		const size_t count = constant_size/size; /* 1GB */
		qarray *a = qarray_create(count, size, disttypes[dt_index]);

		global_size = size;
		qtimer_start(timer);
		for (unsigned i=0;i<10;i++) {
		    qarray_iter_loop(me, a, 0, count, assign1_loop, NULL);
		}
		qtimer_stop(timer);
		printf("\t%lu, %f", (unsigned long)sizes[size_i], qtimer_secs(timer)/10.0);
		fflush(stdout);
		qtimer_start(timer);
		for (unsigned i=0;i<10;i++) {
		    qarray_iter_loop(me, a, 0, count, assert1_loop, NULL);
		}
		qtimer_stop(timer);
		printf(", %f\n", qtimer_secs(timer)/10.0);
		fflush(stdout);
		qarray_destroy(a);
	    }
	}
    }

    qtimer_free(timer);
    qthread_finalize();
    return 0;
}
