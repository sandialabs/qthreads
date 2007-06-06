#include <qthread/qthread.h>
#include <qthread/futurelib.h>

#ifndef QTHREAD_QLOOP
#define QTHREAD_QLOOP

/* for convenient arguments to qt_loop_future */
typedef void (*qt_loop_f) (qthread_t * me, const size_t startat,
			   const size_t stopat, void *arg);
typedef void (*qt_loopr_f) (qthread_t * me, const size_t startat,
			    const size_t stopat, void *arg, void *ret);
typedef void (*qt_accum_f) (void *a, void *b);

void qt_loop(const size_t start, const size_t stop, const size_t stride,
	     const qthread_f func, void *argptr);
void qt_loop_future(const size_t start, const size_t stop,
		    const size_t stride, const qthread_f func, void *argptr);
void qt_loop_balance(const size_t start, const size_t stop,
		     const qt_loop_f func, void *argptr);
void qt_loop_balance_future(const size_t start, const size_t stop,
			    const qt_loop_f func, void *argptr);
void qt_loopaccum_balance(const size_t start, const size_t stop,
			  const size_t size, void *out, const qt_loopr_f func,
			  void *argptr, const qt_accum_f acc);
void qt_loopaccum_balance_future(const size_t start, const size_t stop,
				 const size_t size, void *out,
				 const qt_loopr_f func, void *argptr,
				 const qt_accum_f acc);

double qt_double_sum(double *array, size_t length, int checkfeb);
double qt_double_prod(double *array, size_t length, int checkfeb);
double qt_double_max(double *array, size_t length, int checkfeb);
double qt_double_min(double *array, size_t length, int checkfeb);

int qt_int_sum(int *array, size_t length, int checkfeb);
int qt_int_prod(int *array, size_t length, int checkfeb);
int qt_int_max(int *array, size_t length, int checkfeb);
int qt_int_min(int *array, size_t length, int checkfeb);

unsigned int qt_uint_sum(unsigned int *array, size_t length, int checkfeb);
unsigned int qt_uint_prod(unsigned int *array, size_t length, int checkfeb);
unsigned int qt_uint_max(unsigned int *array, size_t length, int checkfeb);
unsigned int qt_uint_min(unsigned int *array, size_t length, int checkfeb);
#endif
