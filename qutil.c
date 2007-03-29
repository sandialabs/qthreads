#include <qthread/qutil.h>
#include <qthread/qthread.h>
#include <qthread/futurelib.h>

#include <stdlib.h>
#include <stdio.h>		       /* debugging only */
#include <string.h>
#include <assert.h>

#ifndef MT_LOOP_CHUNK
#define MT_LOOP_CHUNK 1000
#endif

#define STRUCT(_structname_, _rtype_) struct _structname_ \
{ \
    const _rtype_ *array; \
    _rtype_ ret; \
    size_t start, stop; \
    const _rtype_ *addlast; \
    struct _structname_ *backptr; \
};
#define INNER_LOOP(_fname_,_structtype_,_opmacro_) static aligned_t _fname_(qthread_t *me, struct _structtype_ *args) \
{ \
    size_t i; \
    args->ret = args->array[args->start]; \
    for (i = args->start + 1; i < args->stop; i++) { \
	_opmacro_(args->ret, args->array[i]); \
    } \
    if (args->addlast) { \
	qthread_readFF(me, NULL, args->addlast); \
	_opmacro_(args->ret, *(args->addlast)); \
	free(args->backptr); \
    } \
    qthread_fill(me, &(args->ret)); \
    return 0; \
}
#define INNER_LOOP_FF(_fname_,_structtype_,_opmacro_) static aligned_t _fname_(qthread_t *me, struct _structtype_ *args) \
{ \
    size_t i; \
    qthread_readFF(me, NULL, args->array + args->start); \
    args->ret = args->array[args->start]; \
    for (i = args->start + 1; i < args->stop; i++) { \
	qthread_readFF(me, NULL, args->array + i); \
	_opmacro_(args->ret, args->array[i]); \
    } \
    if (args->addlast) { \
	qthread_readFF(me, NULL, args->addlast); \
	_opmacro_(args->ret, *(args->addlast)); \
	free(args->backptr); \
    } \
    qthread_fill(me, &(args->ret)); \
    return 0; \
}
#define OUTER_LOOP(_fname_,_structtype_,_opmacro_,_rtype_,_innerfunc_,_innerfuncff_) \
_rtype_ _fname_(qthread_t *me, _rtype_ *array, size_t length, int checkfeb) \
{ \
    size_t i, start = 0; \
    _rtype_ *waitfor = NULL, myret = 0; \
    struct _structtype_ *bkptr = NULL; \
    \
    while (start + MT_LOOP_CHUNK < length) { \
	/* spawn off an MT_LOOP_CHUNK-sized segment of the first part of the array */ \
	struct _structtype_ *left_args = \
	    malloc(sizeof(struct _structtype_)); \
	\
	left_args->array = array; \
	left_args->start = start; \
	left_args->stop = start + MT_LOOP_CHUNK; \
	start += MT_LOOP_CHUNK; \
	left_args->backptr = bkptr; \
	bkptr = left_args; \
	left_args->addlast = waitfor; \
	waitfor = &(left_args->ret); \
	qthread_empty(me, &(left_args->ret)); \
	if (checkfeb) { \
	    future_fork((qthread_f) _innerfuncff_, left_args, NULL); \
	} else { \
	    future_fork((qthread_f) _innerfunc_, left_args, NULL); \
	} \
    } \
    if (checkfeb) { \
	qthread_readFF(me, NULL, array + start); \
	myret = array[start]; \
	for (i = start + 1; i < length; i++) { \
	    qthread_readFF(me, NULL, array + i); \
	    _opmacro_(myret, array[i]); \
	} \
    } else { \
	myret = array[start]; \
	for (i = start + 1; i < length; i++) { \
	    _opmacro_(myret, array[i]); \
	} \
    } \
    if (waitfor) { \
	qthread_readFF(me, NULL, waitfor); \
	_opmacro_(myret, *waitfor); \
	free(bkptr); \
    } \
    return myret; \
}

#define SUM_MACRO(sum,add) sum += (add)
#define MULT_MACRO(prod,factor) prod *= (factor)
#define MAX_MACRO(max, contender) if (max < (contender)) max = (contender)
#define MIN_MACRO(max, contender) if (max > (contender)) max = (contender)

/* These are the functions for computing things about doubles */
STRUCT(qutil_ds_args, double)
INNER_LOOP   (qutil_double_sum_inner,    qutil_ds_args, SUM_MACRO)
INNER_LOOP_FF(qutil_double_FF_sum_inner, qutil_ds_args, SUM_MACRO)
OUTER_LOOP(qutil_double_sum, qutil_ds_args, SUM_MACRO, double, qutil_double_sum_inner, qutil_double_FF_sum_inner)

INNER_LOOP   (qutil_double_mult_inner,    qutil_ds_args, MULT_MACRO)
INNER_LOOP_FF(qutil_double_FF_mult_inner, qutil_ds_args, MULT_MACRO)
OUTER_LOOP(qutil_double_mult, qutil_ds_args, MULT_MACRO, double, qutil_double_mult_inner, qutil_double_FF_mult_inner)

INNER_LOOP   (qutil_double_max_inner,    qutil_ds_args, MAX_MACRO)
INNER_LOOP_FF(qutil_double_FF_max_inner, qutil_ds_args, MAX_MACRO)
OUTER_LOOP(qutil_double_max, qutil_ds_args, MAX_MACRO, double, qutil_double_max_inner, qutil_double_FF_max_inner)

INNER_LOOP   (qutil_double_min_inner,    qutil_ds_args, MIN_MACRO)
INNER_LOOP_FF(qutil_double_FF_min_inner, qutil_ds_args, MIN_MACRO)
OUTER_LOOP(qutil_double_min, qutil_ds_args, MIN_MACRO, double, qutil_double_min_inner, qutil_double_FF_min_inner)

/* These are the functions for computing things about unsigned ints */
STRUCT(qutil_uis_args, unsigned int)
INNER_LOOP   (qutil_uint_sum_inner,    qutil_uis_args, SUM_MACRO)
INNER_LOOP_FF(qutil_uint_FF_sum_inner, qutil_uis_args, SUM_MACRO)
OUTER_LOOP(qutil_uint_sum, qutil_uis_args, SUM_MACRO, unsigned int, qutil_uint_sum_inner, qutil_uint_FF_sum_inner)

INNER_LOOP   (qutil_uint_mult_inner,    qutil_uis_args, MULT_MACRO)
INNER_LOOP_FF(qutil_uint_FF_mult_inner, qutil_uis_args, MULT_MACRO)
OUTER_LOOP(qutil_uint_mult, qutil_uis_args, MULT_MACRO, unsigned int, qutil_uint_mult_inner, qutil_uint_FF_mult_inner)

INNER_LOOP   (qutil_uint_max_inner,    qutil_uis_args, MAX_MACRO)
INNER_LOOP_FF(qutil_uint_FF_max_inner, qutil_uis_args, MAX_MACRO)
OUTER_LOOP(qutil_uint_max, qutil_uis_args, MAX_MACRO, unsigned int, qutil_uint_max_inner, qutil_uint_FF_max_inner)

INNER_LOOP   (qutil_uint_min_inner,    qutil_uis_args, MIN_MACRO)
INNER_LOOP_FF(qutil_uint_FF_min_inner, qutil_uis_args, MIN_MACRO)
OUTER_LOOP(qutil_uint_min, qutil_uis_args, MIN_MACRO, unsigned int, qutil_uint_min_inner, qutil_uint_FF_min_inner)

/* These are the functions for computing things about signed ints */
STRUCT(qutil_is_args, int)
INNER_LOOP   (qutil_int_sum_inner,    qutil_is_args, SUM_MACRO)
INNER_LOOP_FF(qutil_int_FF_sum_inner, qutil_is_args, SUM_MACRO)
OUTER_LOOP(qutil_int_sum, qutil_is_args, SUM_MACRO, int, qutil_int_sum_inner, qutil_int_FF_sum_inner)

INNER_LOOP   (qutil_int_mult_inner,    qutil_is_args, MULT_MACRO)
INNER_LOOP_FF(qutil_int_FF_mult_inner, qutil_is_args, MULT_MACRO)
OUTER_LOOP(qutil_int_mult, qutil_is_args, MULT_MACRO, int, qutil_int_mult_inner, qutil_int_FF_mult_inner)

INNER_LOOP   (qutil_int_max_inner,    qutil_is_args, MAX_MACRO)
INNER_LOOP_FF(qutil_int_FF_max_inner, qutil_is_args, MAX_MACRO)
OUTER_LOOP(qutil_int_max, qutil_is_args, MAX_MACRO, int, qutil_int_max_inner, qutil_int_FF_max_inner)

INNER_LOOP   (qutil_int_min_inner,    qutil_is_args, MIN_MACRO)
INNER_LOOP_FF(qutil_int_FF_min_inner, qutil_is_args, MIN_MACRO)
OUTER_LOOP(qutil_int_min, qutil_is_args, MIN_MACRO, int, qutil_int_min_inner, qutil_int_FF_min_inner)

struct qutil_oetsort_args
{
    double *array;
    size_t start, stop;
};

aligned_t qutil_oetsort_inner(qthread_t *me, struct qutil_oetsort_args *args)
{
    printf("inner: %i to %i\n", args->start, args->stop);
    return 0;
}

void qutil_oetsort(qthread_t *me, double *array, size_t length, int checkfeb)
{
    /* first, decide how much of the array each thread gets */
    size_t chunksize = 10; // could also use MT_LOOP_CHUNK
    /* first, decide how many threads to use... */
    size_t numthreads = length/chunksize; /* could also use numshepherds*2 or something like that */
    aligned_t *rets;
    size_t i;
    struct qutil_oetsort_args *args;

    /* spawn numthreads threads, and wait for them to complete */
    rets = malloc(numthreads * sizeof(aligned_t));
    args = malloc(numthreads * sizeof(struct qutil_oetsort_args));
    for (i=0;i<numthreads; i++) {
	args[i].array = array;
	args[i].start = chunksize * i;
	if ((chunksize+2)*i > length) {
	    /* the last thread's chunk might have to be bigger than the others */
	    args[i].stop = length;
	} else {
	    args[i].stop = (chunksize+1) * i;
	}
	qthread_fork((qthread_f)qutil_oetsort_inner, args+i, rets+i);
    }
    for (i=0;i<numthreads;i++) {
	qthread_readFF(me, NULL, rets+i);
    }
}
