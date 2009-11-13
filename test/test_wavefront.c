#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/wavefront.h>
#include "argparsing.h"

static size_t ASIZE = 2000;

void sum(const void *restrict left, const void *restrict leftdown,
	 const void *restrict down, void *restrict out)
{
    if ((*(int*)left) + (*(int*)leftdown) + (*(int*)down) < 2)
	*(int*)out = 1;
    else
	*(int*)out = 0;
    //*(int *)out = *(int *)left + *(int *)leftdown + *(int *)down;
}

void suma(const void *restrict left, const void *restrict leftdown,
	  const void *restrict down, void *restrict out)
{
    *(aligned_t *) out =
	*(aligned_t *) left + *(aligned_t *) leftdown + *(aligned_t *) down;
}

double ** bigR = NULL;
/* note that the bottom-left corner is item 0 of the below array */
void average(qarray * restrict left, qarray * restrict below, void **R)
{
    bigR=(double**)R;
    /* the first column */
    bigR[0][0] = (*(double*)qarray_elem_nomigrate(left, 0) +
		  *(double*)qarray_elem_nomigrate(below, 0) +
		  *(double*)qarray_elem_nomigrate(below, 1)) / 3.0;
    for (size_t row=1; row < left->count; row++) {
	bigR[0][row] = (*(double*)qarray_elem_nomigrate(left, row) + *(double*)qarray_elem_nomigrate(left, row-1) + bigR[0][row-1])/3.0;
    }
    /* the rest of the columns */
    for (size_t col=1; col < below->count-1; col++) {
	bigR[col][0] = (bigR[col-1][0] /* left */ +
		*(double*)qarray_elem_nomigrate(below, col) /* belowleft */ +
		*(double*)qarray_elem_nomigrate(below, col+1))/3.0;
	for (size_t row=1; row < left->count; row++) {
	    bigR[col][row] = (bigR[col-1][row] +
			      bigR[col-1][row-1] +
			      bigR[col][row-1]) / 3.0;
	}
    }
}

void avg(const void *restrict left, const void *restrict leftdown,
	 const void *restrict down, void *restrict out)
{
    *(double*)out = (*(double*)left + *(double*)leftdown + *(double*)down) / 3.0;
}

void assignrand(qthread_t * me, const size_t startat, const size_t stopat,
	     qarray * a, void *arg)
{
    double *ptr = qarray_elem_nomigrate(a, startat);
    const size_t max = stopat - startat;

    assert(a->unit_size == sizeof(double));

    for (size_t i = 0; i < max; i++) {
	/*long r = random();
	memcpy(&(ptr[i]), &r, sizeof(long));*/
	ptr[i] = (double)(i+startat);
    }
}

int main(int argc, char *argv[])
{
    qthread_t *me;
    qarray *v, *h;
    qt_wavefront_lattice *L;
    char *str;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(ASIZE, "TEST_ASIZE");
    iprintf("ASIZE: %i\n", (int)ASIZE);
    iprintf("%i threads\n", qthread_num_shepherds());

    me = qthread_self();
    v=qarray_create_configured(ASIZE, sizeof(double), FIXED_HASH, 1, 1);
    h=qarray_create_configured(ASIZE+1, sizeof(double), FIXED_HASH, 1, 1);

    qarray_iter_loop(me, h, 1, ASIZE+1, assignrand, NULL);
    qarray_iter_loop(me, v, 0, ASIZE, assignrand, NULL);
    iprintf("v items per seg: %i\n", (int)v->segment_size);

    /* do stuff */
    L = qt_wavefront(v, h, avg);

    if (L) {
	//qt_wavefront_print_lattice(L);
    } else {
	fprintf(stderr,"wavefront returned NULL!\n");
    }
    qarray_destroy(v);
    qarray_destroy(h);
    qt_wavefront_destroy_lattice(L);
    return 0;
}
