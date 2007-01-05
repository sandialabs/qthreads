#include <qthread/qutil.h>
#include <qthread/qthread.h>

#include <stdlib.h>
#include <stdio.h> /* debugging only */
#include <string.h>

struct qsi_args
{
    double *array;
    double ret;
    size_t start, stop;
};

static aligned_t qutil_double_sum_inner(qthread_t *me, struct qsi_args *args)
{
    if (args->start == args->stop) {
	args->ret = args->array[args->start];
	qthread_fill(me, &args->ret, 1);
    } else if (args->start == (args->stop - 1)) {
	args->ret = args->array[args->start] + args->array[args->stop];
	qthread_fill(me, &args->ret, 1);
    } else {
	int leftset = 0, rightset = 0;
	struct qsi_args left_args = {args->array, 0.0, args->start, (args->start+args->stop) >> 1};
	struct qsi_args right_args = {args->array, 0.0, ((args->start+args->stop) >> 1) + 1, args->stop};

	if (left_args.start != left_args.stop) {
	    qthread_empty(me, &left_args.ret, 1);
	    qthread_fork((qthread_f)qutil_double_sum_inner, &left_args, NULL);
	} else {
	    left_args.ret = args->array[left_args.start];
	    leftset = 1;
	}
	if (right_args.start != right_args.stop) {
	    qthread_empty(me, &right_args.ret, 1);
	    qthread_fork((qthread_f)qutil_double_sum_inner, &right_args, NULL);
	} else {
	    right_args.ret = args->array[right_args.start];
	    rightset = 1;
	}
	if (! leftset) {
	    qthread_readFF(me, &left_args.ret, &left_args.ret);
	}
	if (! rightset) {
	    qthread_readFF(me, &right_args.ret, &right_args.ret);
	}
	args->ret = left_args.ret+right_args.ret;
	qthread_fill(me, &args->ret, 1);
    }
    return 0;
}

double qutil_double_sum(qthread_t *me, double * array, size_t length)
{
    struct qsi_args args = {array, 0.0, 0, length-1};
    qthread_empty(me, &args.ret, 1);
    qutil_double_sum_inner(me, &args);
    return args.ret;
}

static aligned_t qutil_double_FF_sum_inner(qthread_t *me, struct qsi_args *args)
{
    if (args->start == args->stop) {
	args->ret = args->array[args->start];
	qthread_fill(me, &args->ret, 1);
    } else if (args->start == (args->stop - 1)) {
	args->ret = args->array[args->start] + args->array[args->stop];
	qthread_fill(me, &args->ret, 1);
    } else {
	int leftset = 0, rightset = 0;
	struct qsi_args left_args = {args->array, 0.0, args->start, (args->start+args->stop) >> 1};
	struct qsi_args right_args = {args->array, 0.0, ((args->start+args->stop) >> 1) + 1, args->stop};

	if (left_args.start != left_args.stop) {
	    qthread_empty(me, &left_args.ret, 1);
	    qthread_fork((qthread_f)qutil_double_FF_sum_inner, &left_args, NULL);
	} else {
	    qthread_readFF(me, &left_args.ret, &(args->array[left_args.start]));
	    left_args.ret = args->array[left_args.start];
	    leftset = 1;
	}
	if (right_args.start != right_args.stop) {
	    qthread_empty(me, &right_args.ret, 1);
	    qthread_fork((qthread_f)qutil_double_FF_sum_inner, &right_args, NULL);
	} else {
	    qthread_readFF(me, &right_args.ret, &(args->array[right_args.start]));
	    right_args.ret = args->array[right_args.start];
	    rightset = 1;
	}
	if (! leftset) {
	    qthread_readFF(me, &left_args.ret, &left_args.ret);
	}
	if (! rightset) {
	    qthread_readFF(me, &right_args.ret, &right_args.ret);
	}
	args->ret = left_args.ret + right_args.ret;
	qthread_fill(me, &args->ret, 1);
    }
    return 0;
}

double qutil_double_FF_sum(qthread_t *me, double *array, size_t length)
{
    struct qsi_args args = {array, 0.0, 0, length-1};
    qthread_empty(me, &args.ret, 1);
    qutil_double_FF_sum_inner(me, &args);
    return args.ret;
}

struct quisi_args
{
    unsigned int *array;
    unsigned int ret;
    size_t start, stop;
};

static void qutil_uint_sum_inner(qthread_t *me, struct quisi_args *args)
{/*{{{*/
    if (args->start == args->stop) {
	qthread_writeF(me, &args->ret, &(args->array[args->start]));
    } else if (args->start == (args->stop - 1)) {
	qthread_writeF_const(me, &args->ret, args->array[args->start] + args->array[args->stop]);
    } else {
	char rightset = 0, leftset = 0;
	struct quisi_args left_args = {args->array, 0, args->start, (args->start+args->stop) >> 1};
	struct quisi_args right_args = {args->array, 0, ((args->start+args->stop) >> 1) + 1, args->stop};

	if (left_args.start != left_args.stop) {
	    qthread_empty(me, &left_args.ret, 1);
	    qthread_fork((qthread_f)qutil_uint_sum_inner, &left_args, NULL);
	} else {
	    left_args.ret = args->array[left_args.start];
	    leftset = 1;
	}
	if (right_args.start != right_args.stop) {
	    qthread_empty(me, &right_args.ret, 1);
	    qthread_fork((qthread_f)qutil_uint_sum_inner, &right_args, NULL);
	} else {
	    right_args.ret = args->array[right_args.start];
	    rightset = 1;
	}
	if (! leftset) {
	    qthread_readFF(me, &left_args.ret, &left_args.ret);
	}
	if (! rightset) {
	    qthread_readFF(me, &right_args.ret, &right_args.ret);
	}
	qthread_writeF_const(me, &args->ret, left_args.ret+right_args.ret);
    }
    return 0;
}/*}}}*/

unsigned int qutil_uint_sum(qthread_t *me, unsigned int * array, size_t length)
{/*{{{*/
    struct quisi_args args = {array, 0, 0, length-1};
    qthread_empty(me, &args.ret, 1);
    qutil_uint_sum_inner(me, &args);
    return args.ret;
}/*}}}*/

struct qutil_argstruct
{
    int iteration;
    void * userargs;
    double (*func)(qthread_t*, const int, void *);
    double *ret;
};

void qutil_threadwrapper(qthread_t *me, struct qutil_argstruct * arg)
{
    double ret;
    ret = arg->func(me, arg->iteration, arg->userargs);
    memcpy(arg->ret, &ret, sizeof(double));
    qthread_writeF(me, arg->ret, &ret);
}

double qutil_runloop_sum_double(qthread_t *me, double (*func)(qthread_t*, const int, void *), void * argstruct, const int loopstart, const int loopend, const int step)
{
    const int iterations = abs(loopend - loopstart);
    double *retvals = malloc(sizeof(double) * iterations);
    int i;
    double sum;
    struct qutil_argstruct *args = malloc(sizeof(struct qutil_argstruct)*iterations);

    for (i=0; i<iterations; i+=step) {
	qthread_empty(me, retvals+i, 1);
    }
    for (i=0; i<iterations; i+=step) {
	args[i].userargs = argstruct;
	args[i].iteration = i+loopstart;
	args[i].func = func;
	args[i].ret = retvals+i;
	qthread_fork((qthread_f)qutil_threadwrapper, args+i, NULL);
    }
    sum = qutil_double_FF_sum(me, retvals, iterations);
    free(retvals);
    return sum;
}
