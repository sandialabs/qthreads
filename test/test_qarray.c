#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qarray.h>

#define ELEMENT_COUNT 10000

aligned_t count = 0;
typedef struct
{
    char pad[10000];
} bigobj;
typedef struct
{
    char pad[41];
} offsize;

static aligned_t assign1(qthread_t * me, void *arg)
{
    *(double *)arg = 1.0;
    qthread_incr(&count, 1);
    return 0;
}

static aligned_t assignall1(qthread_t * me, void *arg)
{
    memset(arg, 1, sizeof(bigobj));
    qthread_incr(&count, 1);
    return 0;
}

static void assignoff1(qthread_t * me, const size_t startat,
		       const size_t stopat, qarray * q, void *arg)
{
    for (size_t i = startat; i < stopat; i++) {
	void *ptr = qarray_elem_nomigrate(q, i);

	memset(ptr, 1, sizeof(offsize));
    }
    qthread_incr(&count, stopat - startat);
}

int main(int argc, char *argv[])
{
    qarray *a;
    int threads = 1;
    qthread_t *me;
    distribution_t disttypes[] = {
	FIXED_HASH, ALL_LOCAL, ALL_RAND, ALL_LEAST, DIST_RAND,
	DIST_REG_STRIPES, DIST_REG_FIELDS, DIST_LEAST
    };
    const char *distnames[] = {
	"FIXED_HASH", "ALL_LOCAL", "ALL_RAND", "ALL_LEAST", "DIST_RAND",
	"DIST_REG_STRIPES", "DIST_REG_FIELDS", "DIST_LEAST"
    };
    int dt_index;
    int interactive = 0;

    if (argc == 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 1;
	    interactive = 0;
	} else {
	    interactive = 1;
	}
    }

    qthread_init(threads);
    me = qthread_self();

    /* iterate over all the different distribution types */
    for (dt_index = 0;
	 dt_index < (sizeof(disttypes) / sizeof(distribution_t));
	 dt_index++) {
	/* test a basic array of doubles */
	count = 0;
	a = qarray_create_configured(ELEMENT_COUNT, sizeof(double),
				     disttypes[dt_index], 0, 0);
	assert(a);
	if (interactive)
	    printf("%s: created basic array of doubles\n",
		   distnames[dt_index]);
	qarray_iter(me, a, 0, ELEMENT_COUNT, assign1);
	if (interactive)
	    printf("%s: iterated; now checking work...\n",
		   distnames[dt_index]);
	if (count != ELEMENT_COUNT) {
	    printf("count = %lu, dt_index = %i\n", (unsigned long)count,
		   dt_index);
	    assert(count == ELEMENT_COUNT);
	}
	{
	    size_t i;

	    for (i = 0; i < ELEMENT_COUNT; i++) {
		double elem = *(double *)qarray_elem_nomigrate(a, i);

		if (elem != 1.0) {
		    printf
			("element %lu is %f instead of 1.0, disttype = %s\n",
			 (unsigned long)i, elem, distnames[dt_index]);
		    assert(elem == 1.0);
		}
	    }
	}
	if (interactive)
	    printf("%s: correct result!\n", distnames[dt_index]);
	qarray_destroy(a);

	/* now test an array of giant things */
	count = 0;
	a = qarray_create_configured(ELEMENT_COUNT, sizeof(bigobj),
				     disttypes[dt_index], 0, 0);
	if (interactive)
	    printf("%s: created array of big objects\n", distnames[dt_index]);
	qarray_iter(me, a, 0, ELEMENT_COUNT, assignall1);
	if (interactive)
	    printf("%s: iterated; now checking work...\n",
		   distnames[dt_index]);
	if (count != ELEMENT_COUNT) {
	    printf("count = %lu, dt_index = %i\n", (unsigned long)count,
		   dt_index);
	    assert(count == ELEMENT_COUNT);
	}
	{
	    size_t i;

	    for (i = 0; i < ELEMENT_COUNT; i++) {
		char *elem = (char *)qarray_elem_nomigrate(a, i);
		size_t j;

		for (j = 0; j < sizeof(bigobj); j++) {
		    if (elem[j] != 1) {
			printf
			    ("byte %lu of element %lu is %i instead of 1, dt_index = %i\n",
			     (unsigned long)j, (unsigned long)i, elem[j],
			     dt_index);
			assert(elem[j] == 1);
		    }
		}
	    }
	}
	if (interactive)
	    printf("%s: correct result!\n", distnames[dt_index]);
	qarray_destroy(a);

	/* now test an array of weird-sized things */
	count = 0;
	a = qarray_create_configured(ELEMENT_COUNT, sizeof(offsize),
				     disttypes[dt_index], 0, 0);
	if (interactive)
	    printf("%s: created array of odd-sized objects\n",
		   distnames[dt_index]);
	qarray_iter_loop(me, a, 0, ELEMENT_COUNT, assignoff1, NULL);
	if (interactive)
	    printf("%s: iterated; now checking work...\n",
		   distnames[dt_index]);
	if (count != ELEMENT_COUNT) {
	    printf("count = %lu, dt_index = %i\n", (unsigned long)count,
		   dt_index);
	    assert(count == ELEMENT_COUNT);
	}
	{
	    size_t i;

	    for (i = 0; i < ELEMENT_COUNT; i++) {
		char *elem = (char *)qarray_elem_nomigrate(a, i);
		size_t j;

		for (j = 0; j < sizeof(offsize); j++) {
		    if (elem[j] != 1) {
			printf
			    ("byte %lu of element %lu is %i instead of 1, dt_index = %i\n",
			     (unsigned long)j, (unsigned long)i, elem[j],
			     dt_index);
			assert(elem[j] == 1);
		    }
		}
	    }
	}
	if (interactive)
	    printf("%s: correct result!\n", distnames[dt_index]);
	qarray_destroy(a);
    }

    qthread_finalize();
    return 0;
//void *qarray_elem_nomigrate(const qarray * a, const size_t index);
//void *qarray_elem(qthread_t * me, const qarray * a, const size_t index);
}
