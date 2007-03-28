#include <stdlib.h>
#include <stdio.h>
#include <float.h>		       /* for DBL_EPSILON (according to C89) */
#include <math.h>		       /* for fabs() */
#include <assert.h>

#include <qthread/qutil.h>

/*
 * This file tests the qutil functions
 *
 */


aligned_t qmain(qthread_t * me, void *junk)
{
    unsigned int *ui_array;
    unsigned int ui_out, ui_sum_authoritative = 0;
    size_t ui_len = 1000000;
    int *i_array;
    int i_out, i_sum_authoritative = 0, i_mult_authoritative = 1;
    size_t i_len = 1000000;
    double *d_array;
    double d_out, d_sum_authoritative = 0.0, d_mult_authoritative = 1.0;
    size_t d_len = 1000000;

    size_t i;

    ui_array = calloc(ui_len, sizeof(unsigned int));
    for (i = 0; i < ui_len; i++) {
	ui_sum_authoritative += ui_array[i] = random();
    }
    ui_out = qutil_uint_sum(me, ui_array, ui_len, 0);
    assert(ui_out == ui_sum_authoritative);
    ui_out = qutil_uint_sum(me, ui_array, ui_len, 1);
    assert(ui_out == ui_sum_authoritative);
    free(ui_array);

    i_array = calloc(i_len, sizeof(int));
    for (i = 0; i < i_len; i++) {
	i_array[i] = random();
	i_sum_authoritative += i_array[i];
	i_mult_authoritative *= i_array[i];
    }
    i_out = qutil_int_sum(me, i_array, i_len, 0);
    assert(i_out == i_sum_authoritative);
    i_out = qutil_int_sum(me, i_array, i_len, 1);
    assert(i_out == i_sum_authoritative);
    i_out = qutil_int_mult(me, i_array, i_len, 0);
    assert(i_out == i_mult_authoritative);
    free(i_array);

    d_array = calloc(d_len, sizeof(double));
    for (i = 0; i < d_len; i++) {
	d_array[i] = random() / (double)RAND_MAX;
	d_sum_authoritative += d_array[i];
	d_mult_authoritative *= d_array[i];
    }
    d_out = qutil_double_sum(me, d_array, d_len, 0);
    assert(fabs(d_out - d_sum_authoritative) < FLT_EPSILON);
    d_out = qutil_double_sum(me, d_array, d_len, 1);
    assert(fabs(d_out - d_sum_authoritative) < FLT_EPSILON);
    d_out = qutil_double_mult(me, d_array, d_len, 0);
    assert(fabs(d_out - d_mult_authoritative) < FLT_EPSILON);
    free(d_array);

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t ret;

    qthread_init(7);
    future_init(2);
    qthread_fork(qmain, NULL, &ret);
    qthread_readFF(NULL, NULL, &ret);
    qthread_finalize();
    return 0;
}
