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
#define INNER_LOOP(_fname_,_structtype_,_op_) static aligned_t _fname_(qthread_t *me, struct _structtype_ *args) \
{ \
    size_t i; \
    args->ret = args->array[args->start]; \
    for (i = args->start + 1; i < args->stop; i++) { \
	args->ret _op_ args->array[i]; \
    } \
    if (args->addlast) { \
	qthread_readFF(me, NULL, args->addlast); \
	args->ret _op_ *(args->addlast); \
	free(args->backptr); \
    } \
    qthread_fill(me, &(args->ret)); \
    return 0; \
}
#define INNER_LOOP_FF(_fname_,_structtype_,_op_) static aligned_t _fname_(qthread_t *me, struct _structtype_ *args) \
{ \
    size_t i; \
    qthread_readFF(me, NULL, args->array + args->start); \
    args->ret = args->array[args->start]; \
    for (i = args->start + 1; i < args->stop; i++) { \
	qthread_readFF(me, NULL, args->array + i); \
	args->ret _op_ args->array[i]; \
    } \
    if (args->addlast) { \
	qthread_readFF(me, NULL, args->addlast); \
	args->ret _op_ *(args->addlast); \
	free(args->backptr); \
    } \
    qthread_fill(me, &(args->ret)); \
    return 0; \
}
#define OUTER_LOOP(_fname_,_structtype_,_op_,_rtype_,_innerfunc_,_innerfuncff_) \
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
	    myret _op_ array[i]; \
	} \
    } else { \
	myret = array[start]; \
	for (i = start + 1; i < length; i++) { \
	    myret _op_ array[i]; \
	} \
    } \
    if (waitfor) { \
	qthread_readFF(me, NULL, waitfor); \
	myret _op_ *waitfor; \
	free(bkptr); \
    } \
    return myret; \
}

STRUCT(qutil_ds_args, double)
INNER_LOOP   (qutil_double_sum_inner,    qutil_ds_args, +=)
INNER_LOOP_FF(qutil_double_FF_sum_inner, qutil_ds_args, +=)
OUTER_LOOP(qutil_double_sum, qutil_ds_args, +=, double, qutil_double_sum_inner, qutil_double_FF_sum_inner)

INNER_LOOP   (qutil_double_mult_inner,    qutil_ds_args, *=)
INNER_LOOP_FF(qutil_double_FF_mult_inner, qutil_ds_args, *=)
OUTER_LOOP(qutil_double_mult, qutil_ds_args, *=, double, qutil_double_mult_inner, qutil_double_FF_mult_inner)

STRUCT(qutil_uis_args, unsigned int)
INNER_LOOP   (qutil_uint_sum_inner,    qutil_uis_args, +=)
INNER_LOOP_FF(qutil_uint_FF_sum_inner, qutil_uis_args, +=)
OUTER_LOOP(qutil_uint_sum, qutil_uis_args, +=, unsigned int, qutil_uint_sum_inner, qutil_uint_FF_sum_inner)

INNER_LOOP   (qutil_uint_mult_inner,    qutil_uis_args, *=)
INNER_LOOP_FF(qutil_uint_FF_mult_inner, qutil_uis_args, *=)
OUTER_LOOP(qutil_uint_mult, qutil_uis_args, *=, unsigned int, qutil_uint_mult_inner, qutil_uint_FF_mult_inner)

STRUCT(qutil_is_args, int)
INNER_LOOP   (qutil_int_sum_inner,    qutil_is_args, +=)
INNER_LOOP_FF(qutil_int_FF_sum_inner, qutil_is_args, +=)
OUTER_LOOP(qutil_int_sum, qutil_is_args, +=, int, qutil_int_sum_inner, qutil_int_FF_sum_inner)

INNER_LOOP   (qutil_int_mult_inner,    qutil_is_args, *=)
INNER_LOOP_FF(qutil_int_FF_mult_inner, qutil_is_args, *=)
OUTER_LOOP(qutil_int_mult, qutil_is_args, *=, int, qutil_int_mult_inner, qutil_int_FF_mult_inner)

