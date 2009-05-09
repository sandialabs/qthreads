#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/allpairs.h>

#define ASIZE 1026

void printout(int *restrict * restrict out)
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

void mult(const int *inta, const int *intb, int *restrict out)
{
    assert(*out == -1);
    *out = (*inta) * (*intb);
}

int main(int argc, char *argv[])
{
    qarray *a1, *a2;
    int **out;
    size_t i;

    qthread_init(0);
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

    qt_allpairs(a1, a2, (void **)out, sizeof(int), (dist_f) mult);
    printout(out);
    for (i = 0; i < ASIZE; i++) {
	free(out[i]);
    }
    free(out);

    qarray_destroy(a1);
    qarray_destroy(a2);

    qthread_finalize();
    return 0;
}
