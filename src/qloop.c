#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "qthread_innards.h"

/* So, the idea here is that this is a (braindead) C version of Megan's
 * mt_loop. */
struct qt_loop_wrapper_args
{
    qthread_f func;
    void *arg;
    volatile aligned_t *donecount;
};

static aligned_t qt_loop_wrapper(qthread_t * me,
				 const struct qt_loop_wrapper_args *arg)
{
    arg->func(me, arg->arg);
    qthread_incr(arg->donecount, 1);
    return 0;
}
static void qt_loop_inner(const size_t start, const size_t stop,
			  const size_t stride, const qthread_f func,
			  void *argptr, int future)
{
    size_t i, threadct = 0;
    qthread_t *me = qthread_self();
    aligned_t *rets;
    size_t steps = (stop - start) / stride;
    volatile aligned_t donecount = 0;
    struct qt_loop_wrapper_args *qwa;

    if ((steps * stride) + start < stop) {
	steps++;
    }
    rets = (aligned_t *) malloc(sizeof(aligned_t) * steps);
    qwa =
	(struct qt_loop_wrapper_args *)
	malloc(sizeof(struct qt_loop_wrapper_args) * steps);

    for (i = start; i < stop; i += stride) {
	qwa[threadct].func = func;
	qwa[threadct].arg = argptr;
	qwa[threadct].donecount = &donecount;
	if (future) {
	    qthread_fork_future_to((qthread_f) qt_loop_wrapper,
				   qwa + threadct, rets + threadct,
				   threadct % qthread_shepherd_count());
	} else {
	    qthread_fork_to((qthread_f) qt_loop_wrapper, qwa + threadct,
			    rets + threadct,
			    threadct % qthread_shepherd_count());
	}
	threadct++;
    }
    for (i = 0; donecount < steps; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    free(qwa);
    free(rets);
}
void qt_loop(const size_t start, const size_t stop, const size_t stride,
	     const qthread_f func, void *argptr)
{
    qt_loop_inner(start, stop, stride, func, argptr, 0);
}

/* So, the idea here is that this is a (braindead) C version of Megan's
 * mt_loop_future. */
void qt_loop_future(const size_t start, const size_t stop,
		    const size_t stride, const qthread_f func, void *argptr)
{
    qt_loop_inner(start, stop, stride, func, argptr, 1);
}

/* So, the idea here is that this is a C version of Megan's mt_loop (note: not
 * using futures here (at least, not exactly)). As such, what it needs to do is
 * decide the following:
 * 1. How many threads will we need?
 * 2. How will the work be divided among the threads?
 *
 * The easy option is this: numthreads is the number of shepherds, and we
 * divide work evenly, so that we're maxing out our processor use with minimal
 * overhead. Then, we divvy the work up as evenly as we can.
 */
struct qloop_wrapper_args
{
    qt_loop_f func;
    size_t startat, stopat;
    void *arg;
    volatile aligned_t *donecount;
};

static aligned_t qloop_wrapper(qthread_t * me,
			       const struct qloop_wrapper_args *arg)
{
    arg->func(me, arg->startat, arg->stopat, arg->arg);
    qthread_incr((aligned_t *) arg->donecount, 1);
    return 0;
}

static inline void qt_loop_balance_inner(const size_t start,
					 const size_t stop,
					 const qt_loop_f func, void *argptr,
					 const int future)
{
    qthread_shepherd_id_t i;
    struct qloop_wrapper_args *qwa =
	(struct qloop_wrapper_args *)malloc(sizeof(struct qloop_wrapper_args)
					    * qthread_shepherd_count());
    aligned_t *rets =
	(aligned_t *) malloc(sizeof(aligned_t) * qthread_shepherd_count());
    volatile aligned_t donecount = 0;
    size_t len = stop - start;
    size_t each = len / qthread_shepherd_count();
    size_t extra = len - (each * qthread_shepherd_count());
    size_t iterend = start;
    qthread_t *me = qthread_self();

    for (i = 0; i < qthread_shepherd_count(); i++) {
	qwa[i].func = func;
	qwa[i].arg = argptr;
	qwa[i].startat = iterend;
	qwa[i].stopat = iterend + each;
	qwa[i].donecount = &donecount;
	if (extra > 0) {
	    qwa[i].stopat++;
	    extra--;
	}
	iterend = qwa[i].stopat;
	if (future) {
	    qthread_fork_future_to((qthread_f) qloop_wrapper, qwa + i,
				   rets + i, i);
	} else {
	    qthread_fork_to((qthread_f) qloop_wrapper, qwa + i, rets + i, i);
	}
    }
    for (i = 0; donecount < qthread_shepherd_count(); i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    free(qwa);
    free(rets);
}
void qt_loop_balance(const size_t start, const size_t stop,
		     const qt_loop_f func, void *argptr)
{
    qt_loop_balance_inner(start, stop, func, argptr, 0);
}
void qt_loop_balance_future(const size_t start, const size_t stop,
			    const qt_loop_f func, void *argptr)
{
    qt_loop_balance_inner(start, stop, func, argptr, 1);
}

struct qloopaccum_wrapper_args
{
    qt_loopr_f func;
    size_t startat, stopat;
    void *restrict arg;
    void *restrict ret;
};

static aligned_t qloopaccum_wrapper(qthread_t * me,
				    const struct qloopaccum_wrapper_args *arg)
{
    arg->func(me, arg->startat, arg->stopat, arg->arg, arg->ret);
    return 0;
}

static inline void qt_loopaccum_balance_inner(const size_t start,
					      const size_t stop,
					      const size_t size,
					      void *restrict out,
					      const qt_loopr_f func,
					      void *restrict argptr,
					      const qt_accum_f acc,
					      const int future)
{
    qthread_shepherd_id_t i;
    struct qloopaccum_wrapper_args *qwa = (struct qloopaccum_wrapper_args *)
	malloc(sizeof(struct qloopaccum_wrapper_args) *
	       qthread_shepherd_count());
    aligned_t *rets =
	(aligned_t *) malloc(sizeof(aligned_t) * qthread_shepherd_count());
    char *realrets = NULL;
    size_t len = stop - start;
    size_t each = len / qthread_shepherd_count();
    size_t extra = len - (each * qthread_shepherd_count());
    size_t iterend = start;
    qthread_t *me = qthread_self();

    if (qthread_shepherd_count() > 1) {
	realrets = (char *)malloc(size * (qthread_shepherd_count() - 1));
    }

    for (i = 0; i < qthread_shepherd_count(); i++) {
	qwa[i].func = func;
	qwa[i].arg = argptr;
	if (i == 0) {
	    qwa[0].ret = out;
	} else {
	    qwa[i].ret = realrets + ((i - 1) * size);
	}
	qwa[i].startat = iterend;
	qwa[i].stopat = iterend + each;
	if (extra > 0) {
	    qwa[i].stopat++;
	    extra--;
	}
	iterend = qwa[i].stopat;
	if (future) {
	    qthread_fork_future_to((qthread_f) qloopaccum_wrapper, qwa + i,
				   rets + i, i);
	} else {
	    qthread_fork_to((qthread_f) qloopaccum_wrapper, qwa + i, rets + i,
			    i);
	}
    }
    for (i = 0; i < qthread_shepherd_count(); i++) {
	qthread_readFF(me, NULL, rets + i);
	if (i > 0) {
	    acc(out, realrets + ((i - 1) * size));
	}
    }
    free(rets);
    free(realrets);
    free(qwa);
}
void qt_loopaccum_balance(const size_t start, const size_t stop,
			  const size_t size, void *restrict out,
			  const qt_loopr_f func, void *restrict argptr,
			  const qt_accum_f acc)
{
    qt_loopaccum_balance_inner(start, stop, size, out, func, argptr, acc, 0);
}
void qt_loopaccum_balance_future(const size_t start, const size_t stop,
				 const size_t size, void *restrict out,
				 const qt_loopr_f func, void *restrict argptr,
				 const qt_accum_f acc)
{
    qt_loopaccum_balance_inner(start, stop, size, out, func, argptr, acc, 1);
}

#define PARALLEL_FUNC(category, initials, _op_, type, shorttype) \
struct qt##initials \
{ \
    type *a; \
    int feb; \
}; \
static void qt##initials##_worker(qthread_t * me, const size_t startat, \
			 const size_t stopat, void * restrict arg, \
			 void * restrict ret) \
{ \
    size_t i; \
    type acc; \
    if (((struct qt##initials *)arg)->feb) { \
	qthread_readFF(me, NULL, ((struct qt##initials *)arg)->a); \
	acc = ((struct qt##initials *)arg)->a[startat]; \
	for (i = startat + 1; i < stopat; i++) { \
	    qthread_readFF(me, NULL, ((struct qt##initials *)arg)->a + i); \
	    acc = _op_(acc, ((struct qt##initials *)arg)->a[i]); \
	} \
    } else { \
	acc = ((struct qt##initials *)arg)->a[startat]; \
	for (i = startat + 1; i < stopat; i++) { \
	    acc = _op_(acc, ((struct qt##initials *)arg)->a[i]); \
	} \
    } \
    *(type *)ret = acc; \
} \
static void qt##initials##_acc (void * restrict a, void * restrict b) \
{ \
    *(type *)a = _op_(*(type *)a, *(type *)b); \
} \
type qt_##shorttype##_##category (type *array, size_t length, int checkfeb) \
{ \
    struct qt##initials arg = { array, checkfeb }; \
    type ret; \
    qt_loopaccum_balance_inner(0, length, sizeof(type), &ret, \
			 qt##initials##_worker, \
			 &arg, qt##initials##_acc, 1 ); \
    return ret; \
}

#define ADD(a,b) a+b
#define MULT(a,b) a*b
#define MAX(a,b) (a>b)?a:b
#define MIN(a,b) (a<b)?a:b

PARALLEL_FUNC(sum, uis, ADD, unsigned int, uint);
PARALLEL_FUNC(prod, uip, MULT, unsigned int, uint);
PARALLEL_FUNC(max, uimax, MAX, unsigned int, uint);
PARALLEL_FUNC(min, uimin, MIN, unsigned int, uint);

PARALLEL_FUNC(sum, is, ADD, int, int);
PARALLEL_FUNC(prod, ip, MULT, int, int);
PARALLEL_FUNC(max, imax, MAX, int, int);
PARALLEL_FUNC(min, imin, MIN, int, int);

PARALLEL_FUNC(sum, ds, ADD, double, double);
PARALLEL_FUNC(prod, dp, MULT, double, double);
PARALLEL_FUNC(max, dmax, MAX, double, double);
PARALLEL_FUNC(min, dmin, MIN, double, double);

/* The next idea is to implement it in a memory-bound kind of way. And I don't
 * mean memory-bound in that it spends its time waiting for memory; I mean in
 * the kind of "that memory belongs to shepherd Y, so therefore iteration X
 * should be on shepherd Y".
 *
 * Of course, in terms of giving each processor a contiguous chunk to work on,
 * that's what qt_loop_balance() does. The really interesting bit is to
 * "register" address ranges with the library, and then have it decide where to
 * spawn threads (& futures) based on the array you've handed it. HOWEVER, this
 * can't be quite so generic, unfortunately, because we don't know what memory
 * we're working with (the qt_loop_balance interface is too generic).
 *
 * The more I think about that, though, the more I'm convinced that it's almost
 * impossible to make particularly generic, because an *arbitrary* function may
 * use two or more arrays that are on different processors, and there's no way
 * qthreads can know that (or even do very much to help if that memory has been
 * assigned to different processors). The best way to achieve this sort of
 * behavior is through premade utility functions, like qutil... but even then,
 * binding given memory to given shepherds won't last through multiple calls
 * unless it's done explicitly. That said, given the way this works, doing
 * repeated operations on the same array will divide the array in the same
 * fashion every time.
 */
#define SWAP(a, m, n) temp=a[m]; a[m]=a[n]; a[n]=temp
static int dcmp(const void *restrict a, const void *restrict b)
{
    if ((*(double *)a) < (*(double *)b))
	return -1;
    if ((*(double *)a) > (*(double *)b))
	return 1;
    return 0;
}

struct qt_qsort_args
{
    double *array;
    double pivot;
    size_t length, chunksize, jump, offset;
    size_t *restrict furthest_leftwall, *restrict furthest_rightwall;
};

aligned_t qt_qsort_partition(qthread_t * me, struct qt_qsort_args *args)
{
    double *a = args->array;
    double temp;
    const double pivot = args->pivot;
    const size_t chunksize = args->chunksize;
    const size_t length = args->length;
    const size_t jump = args->jump;
    size_t leftwall, rightwall;

    leftwall = 0;
    rightwall = length - 1;
    /* adjust the edges; this is critical for this algorithm */
    while (a[leftwall] <= pivot) {
	if ((leftwall + 1) % chunksize != 0) {
	    leftwall++;
	} else {
	    leftwall += jump;
	}
	if (rightwall < leftwall)
	    goto quickexit;
    }
    while (a[rightwall] > pivot) {
	if (rightwall % chunksize != 0) {
	    if (rightwall == 0)
		goto quickexit;
	    rightwall--;
	} else {
	    if (rightwall < jump)
		goto quickexit;
	    rightwall -= jump;
	}
	if (rightwall < leftwall)
	    goto quickexit;
    }
    SWAP(a, leftwall, rightwall);
    while (1) {
	do {
	    leftwall += ((leftwall + 1) % chunksize != 0) ? 1 : jump;
	    if (rightwall < leftwall)
		goto quickexit;
	} while (a[leftwall] <= pivot);
	if (rightwall <= leftwall)
	    break;
	do {
	    if (rightwall % chunksize != 0) {
		if (rightwall == 0)
		    goto quickexit;
		rightwall--;
	    } else {
		if (rightwall < jump)
		    goto quickexit;
		rightwall -= jump;
	    }
	} while (a[rightwall] > pivot);
	if (rightwall <= leftwall)
	    break;
	SWAP(a, leftwall, rightwall);
    }
  quickexit:
    qthread_lock(me, args->furthest_leftwall);
    if (leftwall + args->offset < *args->furthest_leftwall) {
	*args->furthest_leftwall = leftwall + args->offset;
    }
    if (rightwall + args->offset > *args->furthest_rightwall) {
	*args->furthest_rightwall = rightwall + args->offset;
    }
    qthread_unlock(me, args->furthest_leftwall);
    return 0;
}

struct qt_qsort_iargs
{
    double *array;
    size_t length;
};

struct qt_qsort_iprets
{
    size_t leftwall, rightwall;
};

struct qt_qsort_iprets qt_qsort_inner_partitioner(qthread_t * me,
						  double *array,
						  const size_t length,
						  const double pivot)
{
    const size_t chunksize = 10;

    /* choose the number of threads to use */
    const size_t numthreads = qthread_shepherd_count();

    /* a "megachunk" is a set of numthreads chunks.
     * calculate the megachunk information for determining the array lengths
     * each thread will be fed. */
    const size_t megachunk_size = chunksize * numthreads;

    /* just used as a boolean test */
    const size_t extra_chunks = length % megachunk_size;

    /* non-consts */
    size_t megachunks = length / (chunksize * numthreads);
    struct qt_qsort_iprets retval = { ((size_t) - 1), 0 };
    aligned_t *rets;
    struct qt_qsort_args *args;
    size_t i;

    rets = (aligned_t *) malloc(sizeof(aligned_t) * numthreads);
    args =
	(struct qt_qsort_args *)malloc(sizeof(struct qt_qsort_args) *
				       numthreads);
    /* spawn threads to do the partitioning */
    for (i = 0; i < numthreads; i++) {
	args[i].array = array + (i * chunksize);
	args[i].offset = i * chunksize;
	args[i].pivot = pivot;
	args[i].chunksize = chunksize;
	args[i].jump = (numthreads - 1) * chunksize + 1;
	args[i].furthest_leftwall = &retval.leftwall;
	args[i].furthest_rightwall = &retval.rightwall;
	if (extra_chunks != 0) {
	    args[i].length = megachunks * megachunk_size + chunksize;
	    if (args[i].length + args[i].offset >= length) {
		args[i].length = length - args[i].offset;
		megachunks--;
	    }
	} else {
	    args[i].length = length - megachunk_size + chunksize;
	}
	/* qt_qsort_partition(me, args+i); */
	/* future_fork((qthread_f)qt_qsort_partition, args+i, rets+i); */
	qthread_fork((qthread_f) qt_qsort_partition, args + i, rets + i);
    }
    for (i = 0; i < numthreads; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
    free(args);
    free(rets);

    return retval;
}

aligned_t qt_qsort_inner(qthread_t * me, const struct qt_qsort_iargs * a)
{
    const size_t len = a->length;
    double *array = a->array, temp;
    size_t i;
    struct qt_qsort_iprets furthest;
    const size_t thread_chunk = len / qthread_shepherd_count();

    /* choose the number of threads to use */
    if (qthread_shepherd_count() == 1 || len <= 10000) {	/* shortcut */
	qsort(array, len, sizeof(double), dcmp);
	return 0;
    }
    furthest.leftwall = 0;
    furthest.rightwall = len - 1;
    /* tri-median pivot selection */
    i = len / 2;
    if (array[0] > array[i]) {
	SWAP(array, 0, i);
    }
    if (array[0] > array[len - 1]) {
	SWAP(array, 0, len - 1);
    }
    if (array[i] > array[len - 1]) {
	SWAP(array, i, len - 1);
    }
    {
	const double pivot = array[i];

	while (furthest.rightwall > furthest.leftwall &&
	       furthest.rightwall - furthest.leftwall > (2 * thread_chunk)) {
	    const size_t offset = furthest.leftwall;

	    furthest =
		qt_qsort_inner_partitioner(me, array + furthest.leftwall,
					   furthest.rightwall -
					   furthest.leftwall + 1, pivot);
	    furthest.leftwall += offset;
	    furthest.rightwall += offset;
	}
	/* data between furthest.leftwall and furthest.rightwall is unlikely to be partitioned correctly */
	{
	    size_t leftwall = furthest.leftwall, rightwall =
		furthest.rightwall;

	    while (leftwall < rightwall && array[leftwall] <= pivot)
		leftwall++;
	    while (leftwall < rightwall && array[rightwall] > pivot)
		rightwall--;
	    if (leftwall < rightwall) {
		SWAP(array, leftwall, rightwall);
		for (;;) {
		    while (++leftwall < rightwall &&
			   array[leftwall] <= pivot) ;
		    if (rightwall < leftwall)
			break;
		    while (leftwall < --rightwall &&
			   array[rightwall] > pivot) ;
		    if (rightwall < leftwall)
			break;
		    SWAP(array, leftwall, rightwall);
		}
	    }
	    if (array[rightwall] <= pivot) {
		rightwall++;
	    }
	    /* now, spawn the next two iterations */
	    {
		struct qt_qsort_iargs na[2];
		aligned_t rets[2] = { 1, 1 };
		na[0].array = array;
		na[0].length = rightwall;
		na[1].array = array + rightwall;
		na[1].length = len - rightwall;
		if (na[0].length > 0) {
		    rets[0] = 0;
		    /* future_fork((qthread_f)qt_qsort_inner, na, rets); */
		    /* qt_qsort_inner(me, na); */
		    qthread_fork((qthread_f) qt_qsort_inner, na, rets);
		}
		if (na[1].length > 0 && len > rightwall) {
		    rets[1] = 0;
		    /* future_fork((qthread_f)qt_qsort_inner, na+1, rets+1); */
		    /* qt_qsort_inner(me, na+1); */
		    qthread_fork((qthread_f) qt_qsort_inner, na + 1,
				 rets + 1);
		}
		if (rets[0] == 0) {
		    qthread_readFF(me, NULL, rets);
		}
		if (rets[1] == 0) {
		    qthread_readFF(me, NULL, rets + 1);
		}
	    }
	}
    }
    return 0;
}

void qt_qsort(qthread_t * me, double *array, const size_t length)
{
    struct qt_qsort_iargs arg;

    arg.array = array;
    arg.length = length;

    qt_qsort_inner(me, &arg);
}
