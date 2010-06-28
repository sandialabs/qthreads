#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "qthread/qthread.h"
#include "qthread/futurelib.h"
#include "qt_atomics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "futurelib_innards.h"
#include "qthread_innards.h"

/* GLOBAL DATA (copy everywhere) */
pthread_key_t future_bookkeeping;
location_t *future_bookkeeping_array = NULL;

static qthread_shepherd_id_t shep_for_new_futures = 0;
static QTHREAD_FASTLOCK_TYPE sfnf_lock;

/* This function is critical to futurelib, and as such must be as fast as
 * possible.
 *
 * If the qthread is not a future, it returns NULL; otherwise, it returns
 * a pointer to the bookkeeping structure associated with that future's
 * shepherd. */
static location_t *ft_loc(qthread_t * qthr)
{
    return qthread_isfuture(qthr) ? (location_t *)
	pthread_getspecific(future_bookkeeping) : NULL;
}

#ifdef CLEANUP
/* this requires that qthreads haven't been finalized yet */
static aligned_t future_shep_cleanup(qthread_t * me, void *arg)
{
    location_t *ptr = (location_t *) pthread_getspecific(future_bookkeeping);

    if (ptr != NULL) {
	qassert(pthread_setspecific(future_bookkeeping, NULL), 0);
	qassert(pthread_mutex_destroy(&(ptr->vp_count_lock), 0));
	free(ptr);
    }
}

static void future_cleanup(void)
{
    int i;
    aligned_t *rets;

    rets = (aligned_t *) calloc(qlib->nshepherds, sizeof(aligned_t));
    for (i = 0; i < qlib->nshepherds; i++) {
	qthread_fork_to(future_shep_cleanup, NULL, rets + i, i);
    }
    for (i = 0; i < qlib->nshepherds; i++) {
	qthread_readFF(NULL, rets + i, rets + i);
    }
    free(rets);
    QTHREAD_FASTLOCK_DESTROY(sfnf_lock);
    qassert(pthread_key_delete(&future_bookkeeping), 0);
}
#endif

/* this function is used as a qthread; it is run by each shepherd so that each
 * shepherd will get some thread-local data associated with it. This works
 * better in the case of big machines (like massive SMP's) with intelligent
 * pthreads implementations than on PIM, but that's mostly because PIM's libc
 * doesn't support PIM-local data (yet). Better PIM support is coming. */
static aligned_t future_shep_init(qthread_t * me, void * Q_UNUSED arg)
{
    qthread_shepherd_id_t shep = qthread_shep(me);
    location_t *ptr = &(future_bookkeeping_array[shep]);

    // vp_count is *always* locked. This establishes the waiting queue.
    qthread_lock(me, &(ptr->vp_count));

    qassert(pthread_setspecific(future_bookkeeping, ptr), 0);
    return 0;
}

void future_init(int vp_per_loc)
{
    qthread_shepherd_id_t i;
    aligned_t *rets;
    qthread_t *me = qthread_self();

    QTHREAD_FASTLOCK_INIT(sfnf_lock);
    qassert(pthread_key_create(&future_bookkeeping, NULL), 0);
    future_bookkeeping_array =
	(location_t *) calloc(qlib->nshepherds, sizeof(location_t));
    rets = (aligned_t *) calloc(qlib->nshepherds, sizeof(aligned_t));
    for (i = 0; i < qlib->nshepherds; i++) {
	future_bookkeeping_array[i].vp_count = 0;
	future_bookkeeping_array[i].vp_max = vp_per_loc;
	future_bookkeeping_array[i].id = i;
	qassert(pthread_mutex_init
		(&(future_bookkeeping_array[i].vp_count_lock), NULL), 0);
	qthread_fork_to(future_shep_init, NULL, rets + i, i);
    }
    for (i = 0; i < qlib->nshepherds; i++) {
	qthread_readFF(me, rets + i, rets + i);
    }
    free(rets);
#ifdef CLEANUP
    atexit(future_cleanup);
#endif
}

/* This is the heart and soul of the futurelib. This function has two purposes:
 * 1. it checks for (and grabs, if it exists) an available thread-execution
 *    slot
 * 2. if there is no available slot, it adds itself to the waiter
 *    queue to get one.
 */
void blocking_vp_incr(qthread_t * me, location_t * loc)
{
    qassert(pthread_mutex_lock(&(loc->vp_count_lock)), 0);
    qthread_debug(ALL_DETAILS,
		  "thread %p attempting a blocking increment on loc %d vps %d\n",
		  (void *)me, loc->id, loc->vp_count);

    while (loc->vp_count >= loc->vp_max) {
	qassert(pthread_mutex_unlock(&(loc->vp_count_lock)), 0);
	qthread_debug(ALL_DETAILS,
		      "Thread %p found too many futures in %d; waiting for vp_count\n",
		      (void *)me, loc->id);
	qthread_lock(me, &(loc->vp_count));
	qassert(pthread_mutex_lock(&(loc->vp_count_lock)), 0);
    }
    loc->vp_count++;
    qthread_debug(ALL_DETAILS, "Thread %p incr loc %d to %d vps\n", (void *)me, loc->id,
		  loc->vp_count);
    qassert(pthread_mutex_unlock(&(loc->vp_count_lock)), 0);
}

/* creates a qthread, on a location defined by the qthread library, and
 * spawns it when the # of futures on that location is below the specified
 * threshold. Thus, this function has three steps:
 * 1. Figure out where to go
 * 2. Check the # of futures on the destination
 * 3. If there are too many futures there, wait
 */
void future_fork(qthread_f fptr, void *arg, aligned_t * retval)
{
    qthread_shepherd_id_t rr;
    location_t *ptr;
    qthread_t *me;

    assert(future_bookkeeping_array != NULL);
    if (future_bookkeeping_array == NULL) {
	/* futures weren't initialized properly... */
	qthread_fork(fptr, arg, retval);
	return;
    }

    ptr = (location_t *) pthread_getspecific(future_bookkeeping);
    me = qthread_self();

    qthread_debug(THREAD_BEHAVIOR, "Thread %p forking a future\n", (void *)me);
    assert(me != NULL);
    assert(future_bookkeeping_array != NULL);
    /* step 1: future out where to go (fast) */
    /* XXX: should merge with qthread.c to use qthread_internal_incr_mod */
    if (ptr) {
	rr = ptr->sched_shep++;
	ptr->sched_shep *= (ptr->sched_shep < qlib->nshepherds);
    } else {
	QTHREAD_FASTLOCK_LOCK(&sfnf_lock);
	rr = shep_for_new_futures++;
	shep_for_new_futures *= (shep_for_new_futures < qlib->nshepherds);
	QTHREAD_FASTLOCK_UNLOCK(&sfnf_lock);
    }
    qthread_debug(THREAD_DETAILS, "Thread %p decided future will go to %i\n", (void *)me,
		  rr);
    /* steps 2&3 (slow) */
    blocking_vp_incr(me, &(future_bookkeeping_array[rr]));
    qthread_fork_future_to(me, fptr, arg, retval, rr);
}

void future_fork_to(qthread_f fptr, void *arg, aligned_t * retval,
		    qthread_shepherd_id_t shep)
{
    qthread_t *me;

    assert(future_bookkeeping_array != NULL);
    if (future_bookkeeping_array == NULL) {
	/* futures weren't initialized properly... */
	qthread_fork_to(fptr, arg, retval, shep);
	return;
    }

    me = qthread_self();

    qthread_debug(THREAD_BEHAVIOR, "Thread %p forking a future\n", (void *)me);
    assert(me != NULL);
    /* steps 2&3 (slow) */
    blocking_vp_incr(me, &(future_bookkeeping_array[shep]));
    qthread_fork_future_to(me, fptr, arg, retval, shep);
}

void future_fork_syncvar_to(qthread_f fptr, void *arg, syncvar_t * retval,
			    qthread_shepherd_id_t shep)
{
    qthread_t *me;

    assert(future_bookkeeping_array != NULL);
    if (future_bookkeeping_array == NULL) {
	/* futures weren't initialized properly... */
	qthread_fork_syncvar_to(fptr, arg, retval, shep);
	return;
    }

    me = qthread_self();

    qthread_debug(THREAD_BEHAVIOR, "Thread %p forking a future\n", (void *)me);
    assert(me != NULL);
    /* steps 2&3 (slow) */
    blocking_vp_incr(me, &(future_bookkeeping_array[shep]));
    qthread_fork_syncvar_future_to(me, fptr, arg, retval, shep);
}

/* This says: "I do not count toward future resource limits, temporarily." */
int future_yield(qthread_t * me)
{
    location_t *loc;

    assert(me != NULL);
    assert(future_bookkeeping_array != NULL);
    loc = ft_loc(me);
    qthread_debug(THREAD_BEHAVIOR, "Thread %p yield on loc %p\n", (void *)me, (void *)loc);
    //Non-futures do not have a vproc to yield
    if (loc != NULL) {
	int unlockit = 0;

	//yield vproc
	qthread_debug(THREAD_DETAILS, "Thread %p yield loc %d vps %d\n", (void *)me,
		      loc->id, loc->vp_count);
	qassert(pthread_mutex_lock(&(loc->vp_count_lock)), 0);
	unlockit = (loc->vp_count-- == loc->vp_max);
	qassert(pthread_mutex_unlock(&(loc->vp_count_lock)), 0);
	if (unlockit) {
	    qthread_unlock(me, &(loc->vp_count));
	}
	return 1;
    }
    return 0;
}

/* This says: "I count as a future again.", or, more accurately:
 * "I am now a thread that should be limited by the resource limitations."
 */
void future_acquire(qthread_t * me)
{
    location_t *loc;

    assert(me != NULL);
    assert(future_bookkeeping_array != NULL);
    loc = ft_loc(me);
    qthread_debug(THREAD_BEHAVIOR, "Thread %p acquire on loc %p\n", (void *)me,
		  (void *)loc);
    //Non-futures need not acquire a v proc
    if (loc != NULL) {
	blocking_vp_incr(me, loc);
    }
}

/* this is pretty obvious: wait for a thread to finish (ft is supposed
 * to be a thread/future's return value. */
static void future_join(qthread_t * me, aligned_t * ft)
{
    assert(me != NULL);
    assert(future_bookkeeping_array != NULL);
    qthread_debug(THREAD_BEHAVIOR, "Thread %p join to future %p\n", (void *)me, (void *)ft);
    qthread_readFF(me, ft, ft);
}

/* this makes me not a future. Once a future exits, the thread may not
 * terminate, but there's no way for it to become a future again. */
void future_exit(qthread_t * me)
{
    assert(me != NULL);
    assert(future_bookkeeping_array != NULL);
    qthread_debug(THREAD_BEHAVIOR, "Thread %p exit on loc %d\n", (void *)me,
		  qthread_shep(me));
    future_yield(me);
    qthread_assertnotfuture(me);
}

/* a more fun version of future_join */
void future_join_all(qthread_t * qthr, aligned_t * fta, int ftc)
{
    int i;

    assert(qthr != NULL);
    assert(future_bookkeeping_array != NULL);
    assert(fta != NULL);
    assert(ftc > 0);
    qthread_debug(THREAD_BEHAVIOR, "Qthread %p join all to %d futures\n", (void *)qthr,
		  ftc);
    for (i = 0; i < ftc; i++)
	future_join(qthr, fta + i);
}
