#include <stdlib.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "qthread_innards.h"

/* So, the idea here is that this is a (braindead) C version of Megan's
 * mt_loop. */
void qt_loop(const size_t start, const size_t stop, const size_t stride,
	     const qthread_f func, void *argptr)
{
    size_t i, threadct = 0;
    qthread_t *me = qthread_self();
    aligned_t *rets;
    size_t steps = (stop - start)/stride;

    if ((steps * stride) + start < stop) {
	steps ++;
    }
    rets = malloc(sizeof(aligned_t) * steps);

    for (i = start; i < stop; i += stride) {
	qthread_fork_to(func, argptr, rets + threadct,
			threadct % qlib->nshepherds);
	threadct++;
    }
    for (i = 0; i < threadct; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
}

/* So, the idea here is that this is a (braindead) C version of Megan's
 * mt_loop_future. */
void qt_loop_future(const size_t start, const size_t stop,
		    const size_t stride, const qthread_f func, void *argptr)
{
    size_t i, threadct = 0;
    qthread_t *me = qthread_self();
    aligned_t *rets;
    size_t steps = (stop - start)/stride;

    if ((steps * stride) + start < stop) {
	steps ++;
    }
    rets = malloc(sizeof(aligned_t) * steps);

    for (i = start; i < stop; i += stride) {
	qthread_fork_future_to(func, argptr, rets + threadct,
			       threadct % qlib->nshepherds);
	threadct++;
    }
    for (i = 0; i < threadct; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
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
};

static aligned_t qloop_wrapper(qthread_t * me, void *arg)
{
    ((struct qloop_wrapper_args *)arg)->func(me,
					     ((struct qloop_wrapper_args *)
					      arg)->startat,
					     ((struct qloop_wrapper_args *)
					      arg)->stopat,
					     ((struct qloop_wrapper_args *)
					      arg)->arg);
    return 0;
}

static inline void qt_loop_balance_inner(const size_t start,
					 const size_t stop,
					 const qt_loop_f func, void *argptr,
					 const int future)
{
    qthread_shepherd_id_t i;
    struct qloop_wrapper_args *qwa =
	malloc(sizeof(struct qloop_wrapper_args) * qlib->nshepherds);
    aligned_t *rets = malloc(sizeof(aligned_t) * qlib->nshepherds);
    size_t len = stop - start;
    size_t each = len / qlib->nshepherds;
    size_t extra = len - (each * qlib->nshepherds);
    size_t iterend = start;
    qthread_t *me = qthread_self();

    for (i = 0; i < qlib->nshepherds; i++) {
	qwa[i].func = func;
	qwa[i].arg = argptr;
	qwa[i].startat = iterend;
	qwa[i].stopat = iterend + each;
	if (extra > 0) {
	    qwa[i].stopat++;
	    extra--;
	}
	iterend = qwa[i].stopat;
	if (future) {
	    qthread_fork_future_to(qloop_wrapper, qwa + i, rets + i, i);
	} else {
	    qthread_fork_to(qloop_wrapper, qwa + i, rets + i, i);
	}
    }
    for (i = 0; i < qlib->nshepherds; i++) {
	qthread_readFF(me, NULL, rets + i);
    }
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
    void *ret;
    void *arg;
};

static aligned_t qloopaccum_wrapper(qthread_t * me, void *arg)
{
    ((struct qloopaccum_wrapper_args *)arg)->func(me,
						  ((struct
						    qloopaccum_wrapper_args *)
						   arg)->startat,
						  ((struct
						    qloopaccum_wrapper_args *)
						   arg)->stopat,
						  ((struct
						    qloopaccum_wrapper_args *)
						   arg)->arg,
						  ((struct
						    qloopaccum_wrapper_args *)
						   arg)->ret);
    return 0;
}

static inline void qt_loopaccum_balance_inner(const size_t start,
					      const size_t stop,
					      const size_t size, void *out,
					      const qt_loopr_f func,
					      void *argptr,
					      const qt_accum_f acc,
					      const int future)
{
    qthread_shepherd_id_t i;
    struct qloopaccum_wrapper_args *qwa =
	malloc(sizeof(struct qloopaccum_wrapper_args) * qlib->nshepherds);
    aligned_t *rets = malloc(sizeof(aligned_t) * qlib->nshepherds);
    char *realrets = malloc(size * (qlib->nshepherds - 1));
    size_t len = stop - start;
    size_t each = len / qlib->nshepherds;
    size_t extra = len - (each * qlib->nshepherds);
    size_t iterend = start;
    qthread_t *me = qthread_self();

    for (i = 0; i < qlib->nshepherds; i++) {
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
	    qthread_fork_future_to(qloopaccum_wrapper, qwa + i, rets + i, i);
	} else {
	    qthread_fork_to(qloopaccum_wrapper, qwa + i, rets + i, i);
	}
    }
    for (i = 0; i < qlib->nshepherds; i++) {
	qthread_readFF(me, NULL, rets + i);
	if (i > 0) {
	    acc(out, realrets + ((i - 1) * size));
	}
    }
}
void qt_loopaccum_balance(const size_t start, const size_t stop,
			  const size_t size, void *out, const qt_loopr_f func,
			  void *argptr, const qt_accum_f acc)
{
    qt_loopaccum_balance_inner(start, stop, size, out, func, argptr, acc, 0);
}
void qt_loopaccum_balance_future(const size_t start, const size_t stop,
				 const size_t size, void *out,
				 const qt_loopr_f func, void *argptr,
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
			 const size_t stopat, void *arg, void *ret) \
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
static void qt##initials##_acc (void *a, void *b) \
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

PARALLEL_FUNC(sum, uis, ADD, unsigned int, uint)
PARALLEL_FUNC(prod, uip, MULT, unsigned int, uint)
PARALLEL_FUNC(max, uimax, MAX, unsigned int, uint)
PARALLEL_FUNC(min, uimin, MIN, unsigned int, uint)

PARALLEL_FUNC(sum, is, ADD, int, int)
PARALLEL_FUNC(prod, ip, MULT, int, int)
PARALLEL_FUNC(max, imax, MAX, int, int)
PARALLEL_FUNC(min, imin, MIN, int, int)

PARALLEL_FUNC(sum, ds, ADD, double, double)
PARALLEL_FUNC(prod, dp, MULT, double, double)
PARALLEL_FUNC(max, dmax, MAX, double, double)
PARALLEL_FUNC(min, dmin, MIN, double, double)

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
