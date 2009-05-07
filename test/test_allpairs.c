#include <assert.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/allpairs.h>

#define ASIZE 10

void printout(int **out)
{
    size_t i;
    for (i=0; i<ASIZE; i++) {
	size_t j;
	for (j=0; j<ASIZE; j++) {
	    if (out[i][j] == -1) {
		printf("   _ ");
	    } else {
		printf("%4i ", out[i][j]);
	    }
	}
	printf("\n");
    }
}

void mult(const void *inta, const void *intb, void *out)
{
    assert(*(int *)out == -1);
    *(int *)out = (*(int *)inta) * (*(int *)intb);
}

int main(int argc, char *argv[])
{
    int *a;
    int **out;
    size_t i;

    qthread_init(0);
    a = malloc(sizeof(int) * ASIZE);
    for (i = 0; i < ASIZE; i++) {
	a[i] = i;
    }
    out = malloc(sizeof(int *) * ASIZE);
    for (i = 0; i < ASIZE; i++) {
	size_t j;
	out[i] = calloc(sizeof(int), ASIZE);
	for (j = 0; j < ASIZE; j++) {
	    out[i][j] = -1;
	}
    }

    qt_allpairs(a, ASIZE, sizeof(int), (void **)out, sizeof(int), mult);
    printout(out);
    free(a);
    for (i = 0; i < ASIZE; i++) {
	free(out[i]);
    }
    free(out);

    qthread_finalize();
    return 0;
}
