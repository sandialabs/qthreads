#include <qthread/qthread.h>
#include <qthread/futurelib.h>

#ifndef QTHREAD_QLOOP
#define QTHREAD_QLOOP

#ifdef __cplusplus
extern "C"
{
#endif

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

saligned_t qt_int_sum(saligned_t *array, size_t length, int checkfeb);
saligned_t qt_int_prod(saligned_t *array, size_t length, int checkfeb);
saligned_t qt_int_max(saligned_t *array, size_t length, int checkfeb);
saligned_t qt_int_min(saligned_t *array, size_t length, int checkfeb);

aligned_t qt_uint_sum(aligned_t *array, size_t length, int checkfeb);
aligned_t qt_uint_prod(aligned_t *array, size_t length, int checkfeb);
aligned_t qt_uint_max(aligned_t *array, size_t length, int checkfeb);
aligned_t qt_uint_min(aligned_t *array, size_t length, int checkfeb);

/* These are some utility accumulator functions */
static void qt_dbl_add_acc (void *a, void *b)
{
    *(double *)a += *(double *)b;
}
static void qt_int_add_acc (void *a, void *b)
{
    *(int *)a += *(int *)b;
}
static void qt_uint_add_acc (void *a, void *b)
{
    *(unsigned int *)a += *(unsigned int *)b;
}
static void qt_dbl_prod_acc (void *a, void *b)
{
    *(double *)a *= *(double *)b;
}
static void qt_int_prod_acc (void *a, void *b)
{
    *(int *)a *= *(int *)b;
}
static void qt_uint_prod_acc (void *a, void *b)
{
    *(unsigned int *)a *= *(unsigned int *)b;
}
static void qt_dbl_max_acc (void *a, void *b)
{
    if (*(double*)b > *(double*)a)
	*(double *)a = *(double *)b;
}
static void qt_int_max_acc (void *a, void *b)
{
    if (*(int*)b > *(int*)a)
	*(int *)a = *(int *)b;
}
static void qt_uint_max_acc (void *a, void *b)
{
    if (*(unsigned int*)b > *(unsigned int*)a)
	*(unsigned int *)a = *(unsigned int *)b;
}
static void qt_dbl_min_acc (void *a, void *b)
{
    if (*(double*)b < *(double*)a)
	*(double *)a = *(double *)b;
}
static void qt_int_min_acc (void *a, void *b)
{
    if (*(int*)b < *(int*)a)
	*(int *)a = *(int *)b;
}
static void qt_uint_min_acc (void *a, void *b)
{
    if (*(unsigned int*)b < *(unsigned int*)a)
	*(unsigned int *)a = *(unsigned int *)b;
}

#ifdef __cplusplus
}
#endif

#endif
