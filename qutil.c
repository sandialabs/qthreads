#include <qthread/qutil.h>
#include <qthread/qthread.h>
#include <qthread/futurelib.h>

#include <stdlib.h>
#include <stdio.h>		       /* debugging only */
#include <string.h>
#include <assert.h>

#ifndef MT_LOOP_CHUNK
#define MT_LOOP_CHUNK 10000
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

struct qutil_mergesort_args
{
    double * array;
    size_t first_start, first_stop;
    size_t second_start, second_stop;
};

static int dcmp(const void* a, const void* b) {
    if ((*(double*)a) < (*(double*)b)) return -1;
    if ((*(double*)a) > (*(double*)b)) return 1;
    return 0;
}

aligned_t qutil_mergesort_presort(qthread_t *me, struct qutil_mergesort_args *args)
{
    qsort(args->array+args->first_start, args->first_stop-args->first_start+1, sizeof(double), dcmp);
    return 0;
}

aligned_t qutil_mergesort_inner(qthread_t *me, struct qutil_mergesort_args *args)
{
    double * array = args->array;
    while ((args->first_start <= args->first_stop) && (args->second_start <= args->second_stop)) {
	if (array[args->first_start] < array[args->second_start]) {
	    args->first_start++;
	} else {
	    /* XXX: I want a faster in-place merge!!!! */
	    /* The next element comes from the second list,
	     * move the array[second_start] element into the next
	     * position and move all the other elements up
	     */
	    double temp = array[args->second_start];
	    size_t k;
	    for (k = args->second_start - 1; k >= args->first_start; k--) {
		array[k+1] = array[k];
		if (k == 0) break;
	    }
	    array[args->first_start] = temp;
	    args->first_start++;
	    args->first_stop++;
	    args->second_start++;
	}
    }
    return 0;
}

void qutil_mergesort(qthread_t *me, double *array, size_t length, int checkfeb)
{
    /* first, decide how much of the array each thread gets */
    size_t chunksize = MT_LOOP_CHUNK; // could also use MT_LOOP_CHUNK
    /* first, decide how many threads to use... */
    size_t numthreads;
    aligned_t *rets;
    size_t i;
    struct qutil_mergesort_args *args;

    chunksize = 10;
    /* first, an initial qsort() */
    numthreads = length/chunksize;
    if (length - (numthreads*chunksize)) numthreads++;
    rets = malloc(sizeof(aligned_t) * numthreads);
    args = malloc(sizeof(struct qutil_mergesort_args) * numthreads);
    for (i=0;i<numthreads;i++) {
	args[i].array = array;
	args[i].first_start = i*chunksize;
	args[i].first_stop = (i+1)*chunksize-1;
	if (args[i].first_stop >= length) args[i].first_stop = length-1;

	future_fork((qthread_f)qutil_mergesort_presort, args+i,rets+i);
    }
    for (i = 0; i<numthreads;i++) {
	qthread_readFF(me, NULL, rets+i);
    }
    free(rets);
    free(args);
    /* prepare scratch memory */
    if (chunksize <= length) {
	numthreads = (length-chunksize)/(2*chunksize);
	if ((length-chunksize) - (2*chunksize*numthreads)) numthreads++;
	rets = malloc(sizeof(aligned_t) * numthreads);
	assert(rets);
	args = malloc(sizeof(struct qutil_mergesort_args) * numthreads);
	assert(args);
	numthreads = 0;
    }
    /* now, commence with the merging */
    while (chunksize <= length) {
	i = 0;
	numthreads = 0;
	while (i < length-chunksize) {
	    args[numthreads].array = array;
	    args[numthreads].first_start = i;
	    args[numthreads].first_stop = i+chunksize-1;
	    args[numthreads].second_start = i+chunksize;
	    args[numthreads].second_stop = ((i+2*chunksize-1)<(length-1))?(i+2*chunksize-1):(length-1);
	    future_fork((qthread_f)qutil_mergesort_inner, args+numthreads, rets+numthreads);
	    i += 2*chunksize;
	    numthreads++;
	}
	for (i = 0; i<numthreads;i++) {
	    qthread_readFF(me, NULL, rets+i);
	}
	chunksize *= 2;
    }
    if (rets) {
	free(rets);
	free(args);
    }
}

#define SWAP(a, m, n) temp=a[m]; a[m]=a[n]; a[n]=temp

struct qutil_qsort_args
{
    double * array;
    double pivot;
    size_t length, chunksize, jump, offset;
    size_t *furthest_leftwall, *furthest_rightwall;
};

void showarray(double*a, size_t start, size_t stop)
{
    size_t i;
    printf("values\n");
    for (i=start;i<stop;i++) {
	if (i%10 == 0) printf("\n");
	printf("[%4i]=%2.5f ", i, a[i]);
    }
    printf("\n");
}

aligned_t qutil_qsort_partition(qthread_t *me, struct qutil_qsort_args *args)
{
    double * a = args->array;
    double temp;
    const double pivot=args->pivot;
    const size_t chunksize = args->chunksize;
    const size_t length = args->length;
    const size_t jump = args->jump;
    size_t leftwall, cursor, rightwall;

#ifdef EXPERIMENTAL
    leftwall = 0;
    rightwall = length - 1;
    /* sort the edges; this is critical for this algorithm */
    if (a[leftwall] > a[rightwall]) {
	SWAP(a,leftwall,rightwall);
    }
    while (1) {
	do {
	    leftwall += ((leftwall+1)%chunksize != 0)?1:jump;
	} while (a[leftwall] <= pivot);
	do {
	    rightwall -= (rightwall%chunksize != 0)?1:jump;
	} while (a[rightwall] > pivot);
	if (rightwall < leftwall) break;
	SWAP(a,leftwall,rightwall);
    }
    SWAP(a,leftwall,length-1);
#else
    /* the way this works is that it parses from left to right with the cursor.
     * For each number, it goes either to the left-end of the array (leftwall)
     * or the right-end of the array (rightwall) or stays put (if it's equal to
     * the pivot). What you end up with is everything less than the pivot on
     * one side, everything equal to the pivot in the middle, and everything
     * greater than the pivot on the right. This is NOT the fastest quicksort
     * partitioner in the world, because a number can get swapped multiple
     * times, and because you don't *NEED* all the pivots to be in the middle.
     *
     * The above partitioner should be more efficient, but seems to have some
     * minor bugs in it still. */

    /* cray method */
    cursor = leftwall = 0;
    rightwall = length - 1;
    while (cursor <= rightwall) {
	if (a[cursor] < pivot) {
	    SWAP(a, cursor, leftwall);
	    if ((cursor+1) % chunksize != 0) {
		cursor ++;
	    } else {
		cursor += jump;
	    }
	    if ((leftwall+1) % chunksize != 0) {
		leftwall ++;
	    } else {
		leftwall += jump;
	    }
	} else if (a[cursor] == pivot) {
	    if ((cursor+1) % chunksize != 0) {
		cursor ++;
	    } else {
		cursor += jump;
	    }
	} else {
	    SWAP(a, cursor, rightwall);
	    if (rightwall % chunksize != 0) {
		rightwall --;
	    } else {
		if (rightwall < jump) break;
		rightwall -= jump;
	    }
	}
    }
#endif
    qthread_lock(me, args->furthest_leftwall);
    if (leftwall+args->offset < *args->furthest_leftwall) {
	*args->furthest_leftwall = leftwall+args->offset;
    }
    if (rightwall+args->offset > *args->furthest_rightwall) {
	*args->furthest_rightwall = rightwall+args->offset;
    }
    qthread_unlock(me, args->furthest_leftwall);
    return 0;
}

struct qutil_qsort_iargs
{
    double * array;
    const size_t length;
};

aligned_t qutil_qsort_inner(qthread_t *me, struct qutil_qsort_iargs *a)
{
    double *array = a->array, temp, pivot;
    struct qutil_qsort_args *args;
    aligned_t *rets;
    size_t p = a->length/2;
    size_t furthest_leftwall = ((size_t)-1), furthest_rightwall = 0;
    size_t chunksize=10, numthreads, i;
    size_t megachunk_size, megachunks, extra_chunks;

    /* choose the number of threads to use */
    numthreads = a->length/MT_LOOP_CHUNK + ((a->length%MT_LOOP_CHUNK)?1:0);
    if (numthreads == 1) { /* shortcut */
	qsort(array, a->length, sizeof(double), dcmp);
	return 0;
    }
    /* tri-median pivot selection */
    if (array[0] > array[p]) {
	SWAP(array, 0, p);
    }
    if (array[0] > array[a->length-1]) {
	SWAP(array, 0, a->length-1);
    }
    if (array[p] > array[a->length-1]) {
	SWAP(array, p, a->length-1);
    }
    pivot = array[p];
    /* calculate the megachunk information for determining the array lengths
     * each thread will be fed. */
    megachunk_size = chunksize * numthreads;
    megachunks = a->length/(chunksize * numthreads);
    extra_chunks = a->length % megachunk_size; // just used as a boolean test
    rets = malloc(sizeof(aligned_t) * numthreads);
    args = malloc(sizeof(struct qutil_qsort_args) * numthreads);
    /* spawn threads to do the partitioning */
    for (i=0;i<numthreads;i++) {
	args[i].array              = array + (i*chunksize);
	args[i].offset             = i*chunksize;
	args[i].pivot              = pivot;
	args[i].chunksize          = chunksize;
	args[i].jump               = (numthreads-1) * chunksize + 1;
	args[i].furthest_leftwall  = &furthest_leftwall;
	args[i].furthest_rightwall = &furthest_rightwall;
	if (extra_chunks != 0) {
	    args[i].length = megachunks * (megachunk_size) + chunksize;
	    if (args[i].length + args[i].offset >= a->length) {
		args[i].length = a->length - args[i].offset;
		megachunks --;
	    }
	} else {
	    args[i].length = a->length - megachunk_size + chunksize;
	}
	//qutil_qsort_partition(me, args+i);
	//future_fork((qthread_f)qutil_qsort_partition, args+i, rets+i);
	qthread_fork((qthread_f)qutil_qsort_partition, args+i, rets+i);
    }
    for (i=0;i<numthreads;i++) {
	qthread_readFF(me, NULL, rets+i);
    }
    free(args); free(rets);
    /* data between furthest_leftwall and furthest_rightwall is unlikely to be partitioned correctly */
    {
	size_t cursor = furthest_leftwall, leftwall = furthest_leftwall, rightwall = furthest_rightwall;
	while (cursor<=rightwall) {
	    if (array[cursor] < pivot) {
		SWAP(array, cursor, leftwall);
		cursor++; leftwall++;
	    } else if (array[cursor] == pivot) {
		cursor++;
	    } else {
		SWAP(array, cursor, rightwall);
		rightwall--;
	    }
	}
	/* now, spawn the next two iterations */
	{
	    struct qutil_qsort_iargs na[2] = {{array, leftwall}, {array+rightwall+1, a->length-rightwall-1}};
	    aligned_t rets[2] = {1,1};
	    if (na[0].length > 0) {
		rets[0] = 0;
		//future_fork((qthread_f)qutil_qsort_inner, na, rets);
		//qutil_qsort_inner(me, na);
		qthread_fork((qthread_f)qutil_qsort_inner, na, rets);
	    }
	    if (na[1].length > 0 && a->length > rightwall) {
		rets[1] = 0;
		//future_fork((qthread_f)qutil_qsort_inner, na+1, rets+1);
		//qutil_qsort_inner(me, na+1);
		qthread_fork((qthread_f)qutil_qsort_inner, na+1, rets+1);
	    }
	    if (rets[0] == 0) {
		qthread_readFF(me, NULL, rets);
	    }
	    if (rets[1] == 0) {
		qthread_readFF(me, NULL, rets+1);
	    }
	}
    }
    return 0;
}

void qutil_qsort(qthread_t *me, double *array, const size_t length)
{
    struct qutil_qsort_iargs arg = {array, length};

    /*if (length <= MT_LOOP_CHUNK) {
	qsort(array, length, sizeof(double), dcmp);
	return;
    }*/
    qutil_qsort_inner(me, &arg);
}
