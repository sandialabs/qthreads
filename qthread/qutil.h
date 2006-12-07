#ifndef QTHREAD_QUTIL_H
#define QTHREAD_QUTIL_H

#include <stddef.h>

#include <qthread/qthread.h>

#ifdef __cplusplus
extern "C" {
#endif

double qutil_double_sum(qthread_t *me, double * array, size_t length);
unsigned int qutil_uint_sum(qthread_t *me, unsigned int * array, size_t length);
double qutil_double_FF_sum(qthread_t *me, double *array, size_t length);
double qutil_runloop_sum_double(qthread_t *me, double (*func)(qthread_t*, const int, void *), void * argstruct, const int loopstart, const int loopend, const int step);

#ifdef __cplusplus
}
#endif
#endif
