#include <stdlib.h>
#include <stdio.h>
#include <limits.h>		       /* for INT_MIN & friends (according to C89) */
#include <float.h>		       /* for DBL_EPSILON (according to C89) */
#include <math.h>		       /* for fabs() */
#include <assert.h>

#include <sys/time.h>		       /* for gettimeofday() */
#include <time.h>		       /* for gettimeofday() */

#include <qthread/qutil.h>

/*
 * This file tests the qutil functions
 *
 */
static int dcmp(const void* a, const void* b) {
    if ((*(double*)a) < (*(double*)b)) return -1;
    if ((*(double*)a) > (*(double*)b)) return 1;
    return 0;
}


aligned_t qmain(qthread_t * me, void *junk)
{
    unsigned int *ui_array;
    unsigned int ui_out, ui_sum_authoritative = 0, ui_mult_authoritative =
	1, ui_max_authoritative = 0, ui_min_authoritative = UINT_MAX;
    size_t ui_len = 1000000;
    int *i_array;
    int i_out, i_sum_authoritative = 0, i_mult_authoritative =
	1, i_max_authoritative = INT_MIN, i_min_authoritative = INT_MAX;
    size_t i_len = 1000000;
    double *d_array;
    double d_out, d_sum_authoritative = 0.0, d_mult_authoritative =
	1.0, d_max_authoritative = DBL_MIN, d_min_authoritative = DBL_MAX;
    size_t d_len = 100000000;

    size_t i;
    struct timeval start, stop;

    ui_array = calloc(ui_len, sizeof(unsigned int));
    for (i = 0; i < ui_len; i++) {
	ui_array[i] = random();
	ui_sum_authoritative += ui_array[i];
	ui_mult_authoritative *= ui_array[i];
	if (ui_max_authoritative < ui_array[i])
	    ui_max_authoritative = ui_array[i];
	if (ui_min_authoritative > ui_array[i])
	    ui_min_authoritative = ui_array[i];
    }
    ui_out = qutil_uint_sum(me, ui_array, ui_len, 0);
    assert(ui_out == ui_sum_authoritative);
    /* testing with FEB just for full coverage; if it's good for uints, it's
     * good for everyone else */
    ui_out = qutil_uint_sum(me, ui_array, ui_len, 1);
    assert(ui_out == ui_sum_authoritative);
    ui_out = qutil_uint_mult(me, ui_array, ui_len, 0);
    assert(ui_out == ui_mult_authoritative);
    ui_out = qutil_uint_max(me, ui_array, ui_len, 0);
    assert(ui_out == ui_max_authoritative);
    ui_out = qutil_uint_min(me, ui_array, ui_len, 0);
    assert(ui_out == ui_min_authoritative);
    free(ui_array);

    i_array = calloc(i_len, sizeof(int));
    for (i = 0; i < i_len; i++) {
	i_array[i] = random();
	i_sum_authoritative += i_array[i];
	i_mult_authoritative *= i_array[i];
	if (i_max_authoritative < i_array[i])
	    i_max_authoritative = i_array[i];
	if (i_min_authoritative > i_array[i])
	    i_min_authoritative = i_array[i];
    }
    i_out = qutil_int_sum(me, i_array, i_len, 0);
    assert(i_out == i_sum_authoritative);
    i_out = qutil_int_mult(me, i_array, i_len, 0);
    assert(i_out == i_mult_authoritative);
    i_out = qutil_int_max(me, i_array, i_len, 0);
    assert(i_out == i_max_authoritative);
    i_out = qutil_int_min(me, i_array, i_len, 0);
    assert(i_out == i_min_authoritative);
    free(i_array);

    d_array = calloc(d_len, sizeof(double));
    assert(d_array != NULL);
    for (i = 0; i < d_len; i++) {
	d_array[i] = random() / (double)RAND_MAX *10;

	d_sum_authoritative += d_array[i];
	d_mult_authoritative *= d_array[i];
	if (d_max_authoritative < d_array[i])
	    d_max_authoritative = d_array[i];
	if (d_min_authoritative > d_array[i])
	    d_min_authoritative = d_array[i];
    }
    d_out = qutil_double_sum(me, d_array, d_len, 0);
    if (fabs(d_out - d_sum_authoritative) > (fabs(d_out+d_sum_authoritative)*FLT_EPSILON)) {
	printf("unexpectedly large sum delta: %g (EPSILON = %g)\n",
	       fabs(d_out - d_sum_authoritative), (fabs(d_out+d_sum_authoritative)*FLT_EPSILON/2));
	/*printf("d_out = %g\nd_sum_authoritative = %g\n", d_out,
	       d_sum_authoritative);*/
    }
    d_out = qutil_double_mult(me, d_array, d_len, 0);
    if (fabs(d_out - d_mult_authoritative) > fabs(d_out+d_mult_authoritative)*FLT_EPSILON) {
	printf("unexpectedly large mult. delta: %g\n",
	       fabs(d_out - d_mult_authoritative));
    }
    d_out = qutil_double_max(me, d_array, d_len, 0);
    assert(d_out == d_max_authoritative);
    d_out = qutil_double_min(me, d_array, d_len, 0);
    assert(d_out == d_min_authoritative);

    /*qutil_mergesort(me, d_array, d_len, 0);
     * for (i = 0; i < d_len-1; i++) {
     * if (d_array[i] > d_array[i+1]) {
     * printf("out of order at %i: %f > %f\n", i, d_array[i], d_array[i+1]);
     * abort();
     * }
     * } */

    printf("sorting...\n");
    gettimeofday(&start, NULL);
    qutil_qsort(me, d_array, d_len);
    //qsort(d_array, d_len, sizeof(double), dcmp);
    gettimeofday(&stop, NULL);
    for (i = 0; i < d_len - 1; i++) {
	if (d_array[i] > d_array[i + 1]) {
	    size_t j;

	    for (j = 0; j < d_len; j++) {
		if (j % 10 == 0)
		    printf("\n");
		printf("[%4u]=%2.5f ", j, d_array[j]);
	    }
	    printf("\n");
	    printf("out of order at %i: %f > %f\n", i, d_array[i],
		   d_array[i + 1]);
	    abort();
	}
    }
    printf("\n");
    printf("sorting %lu numbers took: %f seconds\n", (unsigned long)d_len,
	   (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
						      (start.tv_usec *
						       1.0e-6)));
    free(d_array);
    printf("SUCCESS!!!\n");
    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;

    qthread_init(3);
    future_init(128);
    qthread_fork(qmain, NULL, &ret);
    qthread_readFF(NULL, NULL, &ret);
    qthread_finalize();
    return 0;
}
