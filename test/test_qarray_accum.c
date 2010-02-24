#include <qthread/qthread.h>
#include <qthread/qarray.h>

#include <stdio.h>
#include <assert.h>

#define ITER 1000000

static void assigni(qthread_t *me, const size_t startat, const size_t stopat, qarray *q, void *arg)
{
    int *ptr = (int*)qarray_elem(me, q, startat);
    size_t i;
    for (i = 0; i < (stopat - startat); ++i) {
	ptr[i] = (int)(i+startat);
    }
}

static void permute(qthread_t *me, const size_t startat, const size_t stopat, qarray *q, void *arg, void *ret)
{
    int *ptr = (int*)qarray_elem(me, q, startat);
    size_t i;
    double sum = 0.0;
    for (i = 0; i < (stopat - startat); ++i) {
	double tmp = (double)ptr[i];
	qt_dbl_add_acc(&sum, &tmp);
    }
    memcpy(ret, &sum, sizeof(double));
}

int main()
{
    qarray *t;
    qthread_t *me;
    double ret = 0.0;
    size_t int_calc = 0, i;

    qthread_initialize();
    me = qthread_self();

    t = qarray_create(ITER, sizeof(int));
    assert(t);
    qarray_iter_loop(me, t, 0, ITER, assigni, NULL);
    for (i = 1; i<ITER; i++) {
	int_calc += i;
    }
    qarray_iter_loopaccum(me, t, 0, ITER, permute, NULL, &ret, sizeof(double), qt_dbl_add_acc);
    printf("int = %lu\n", (long unsigned)int_calc);
    printf("ret = %f\n", ret);
    assert(int_calc == ret);
    qarray_destroy(t);
    return 0;
}
