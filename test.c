#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <qthread/qthread.h>
#include <qthread/qutil.h>

/*
 * This file tests the qutil functions
 *
 */

int main(int argc, char *argv[])
{
    unsigned int *ui_array;
    unsigned int ui_sum, ui_sum_authoritative = 0;
    unsigned int ui_len = 10000;
    double * d_array;
    double d_sum, d_sum_authoritative = 0.0;
    unsigned int d_len = 10000;
    unsigned int i;

    qthread_init(3);

    ui_array = calloc(ui_len, sizeof(unsigned int));
    for (i=0;i<ui_len; i++) {
	ui_array[i] = i+1;
	ui_sum_authoritative += i+1;
    }
    ui_sum = qutil_uint_sum(NULL, ui_array, ui_len);
    assert(ui_sum == ui_sum_authoritative);
    free(ui_array);

    d_array = calloc(d_len, sizeof(double));
    for (i=0;i<d_len; i++) {
	d_array[i] = i+1;
	d_sum_authoritative += i+1;
    }
    d_sum = qutil_double_sum(NULL, d_array, d_len);
    assert(d_sum == d_sum_authoritative);
    d_sum = qutil_double_FF_sum(NULL, d_array, d_len);
    assert(d_sum == d_sum_authoritative);
    free(d_array);

    qthread_finalize();
    return 0;
}
