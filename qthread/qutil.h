#ifndef QTHREAD_QUTIL_H
#define QTHREAD_QUTIL_H

#include <stddef.h>

#include <qthread/qthread.h>
#include <qthread/futurelib.h>

#ifdef __cplusplus
extern "C"
{
#endif
/* This computes the sum/product of all the doubles in an array. If checkfeb is
 * non-zero, then it will wait for each array entry to be marked FEB-full */
double qutil_double_sum(qthread_t * me, double *array, size_t length,
			int checkfeb);
double qutil_double_mult(qthread_t * me, double *array, size_t length,
			 int checkfeb);
double qutil_double_max(qthread_t * me, double *array, size_t length,
			int checkfeb);
double qutil_double_min(qthread_t * me, double *array, size_t length,
			int checkfeb);
/* This computes the sum/product of all the unsigned ints in an array */
unsigned int qutil_uint_sum(qthread_t * me, unsigned int *array,
			    size_t length, int checkfeb);
unsigned int qutil_uint_mult(qthread_t * me, unsigned int *array,
			     size_t length, int checkfeb);
unsigned int qutil_uint_max(qthread_t * me, unsigned int *array,
			    size_t length, int checkfeb);
unsigned int qutil_uint_min(qthread_t * me, unsigned int *array,
			    size_t length, int checkfeb);
/* This computes the sum/product of all the ints in an array */
int qutil_int_sum(qthread_t * me, int *array, size_t length, int checkfeb);
int qutil_int_mult(qthread_t * me, int *array, size_t length, int checkfeb);
int qutil_int_max(qthread_t * me, int *array, size_t length, int checkfeb);
int qutil_int_min(qthread_t * me, int *array, size_t length, int checkfeb);

void qutil_mergesort(qthread_t *me, double *array, size_t length, int checkfeb);
void qutil_qsort(qthread_t *me, double *array, size_t length);
#ifdef __cplusplus
}
#endif
#endif
