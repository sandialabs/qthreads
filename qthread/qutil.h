#ifndef QTHREAD_QUTIL_H
#define QTHREAD_QUTIL_H

#include <stddef.h>

#include <qthread/qthread.h>

#ifdef __cplusplus
extern "C"
{
#endif
/* This computes the sum of all the doubles in an array */
double qutil_double_sum(qthread_t * me, double *array, size_t length);
/* This computes the sum of all the unsigned ints in an array */
unsigned int qutil_uint_sum(qthread_t * me, unsigned int *array,
			    size_t length);
/* This computes the sum of all the doubles in an array that may not all be
 * filled in yet */
double qutil_double_FF_sum(qthread_t * me, double *array, size_t length);

/* and this will run a bunch of threads, each of which returns a double. The
 * return values will be summed up as they are computed. */
double qutil_runloop_sum_double(qthread_t * me,
				double (*func) (qthread_t *, const int,
						void *), void *argstruct,
				const int loopstart, const int loopend,
				const int step);

#ifdef __cplusplus
}
#endif
#endif
