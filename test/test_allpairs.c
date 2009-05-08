#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/allpairs.h>

#define ASIZE 1026

void printout(int **out)
{
    size_t i;
    for (i=0; i<ASIZE; i++) {
	size_t j;
	for (j=0; j<i; j++) {
	    printf("       _ ");
	}
	for (j=0; j<(ASIZE-i); j++) {
	    if (out[i][j] == -1) {
		printf(" _....._ ");
	    } else {
		printf("%8i ", out[i][j]);
	    }
	}
	printf("\n");
    }
}

void mult(const int *inta, const int *intb, int *out)
{
    assert(*out == -1);
    *out = (*inta) * (*intb);
}

int main(int argc, char *argv[])
{
    int *a;
    qarray *a2;
    int **out;
    size_t i;

    qthread_init(0);
    a = malloc(sizeof(int) * ASIZE);
    a2 = qarray_create_tight(ASIZE, sizeof(int), FIXED_HASH);
    for (i = 0; i < ASIZE; i++) {
	a[i] = i;
	*(int*)qarray_elem_nomigrate(a2, i) = i;
    }
    out = calloc(ASIZE, sizeof(int *));
    assert(out);
    for (i = 0; i < ASIZE; i++) {
	size_t j;
	out[i] = calloc(sizeof(int), ASIZE-i);
	assert(out[i]);
	for (j = 0; j < (ASIZE-i); j++) {
	    out[i][j] = -1;
	}
    }

    qt_allpairs(a2, ASIZE, sizeof(int), (void **)out, sizeof(int), (dist_f)mult);
    printout(out);
    free(a);
    for (i = 0; i < ASIZE; i++) {
	free(out[i]);
    }
    free(out);

    qthread_finalize();
    return 0;
}
