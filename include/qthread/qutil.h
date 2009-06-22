#ifndef QTHREAD_QUTIL_H
#define QTHREAD_QUTIL_H

#include <qthread/qthread.h>

Q_STARTCXX /* */

/* This computes the sum/product of all the doubles in an array. If checkfeb is
 * non-zero, then it will wait for each array entry to be marked FEB-full */
double qutil_double_sum(qthread_t * me, const double *array, size_t length,
			int checkfeb);
double qutil_double_mult(qthread_t * me, const double *array, size_t length,
			 int checkfeb);
double qutil_double_max(qthread_t * me, const double *array, size_t length,
			int checkfeb);
double qutil_double_min(qthread_t * me, const double *array, size_t length,
			int checkfeb);
/* This computes the sum/product of all the aligned_ts in an array */
aligned_t qutil_uint_sum(qthread_t * me, const aligned_t * array,
			 size_t length, int checkfeb);
aligned_t qutil_uint_mult(qthread_t * me, const aligned_t * array,
			  size_t length, int checkfeb);
aligned_t qutil_uint_max(qthread_t * me, const aligned_t * array,
			 size_t length, int checkfeb);
aligned_t qutil_uint_min(qthread_t * me, const aligned_t * array,
			 size_t length, int checkfeb);
/* This computes the sum/product of all the saligned_ts in an array */
saligned_t qutil_int_sum(qthread_t * me, const saligned_t * array,
			 size_t length, int checkfeb);
saligned_t qutil_int_mult(qthread_t * me, const saligned_t * array,
			  size_t length, int checkfeb);
saligned_t qutil_int_max(qthread_t * me, const saligned_t * array,
			 size_t length, int checkfeb);
saligned_t qutil_int_min(qthread_t * me, const saligned_t * array,
			 size_t length, int checkfeb);

void qutil_mergesort(qthread_t * me, double *array, size_t length);
void qutil_qsort(qthread_t * me, double *array, size_t length);
void qutil_aligned_qsort(qthread_t * me, aligned_t * array, size_t length);

Q_ENDCXX /* */
#endif
