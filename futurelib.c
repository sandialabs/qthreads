#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "qthread/qthread.h"
#include "qthread/futurelib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cprops/hashtable.h>

#if 0
#define DBprintf printf
#else
#define DBprintf(...) ;
#endif

#define MALLOC(sss) malloc(sss)
#define FREE(sss) free(sss)

#include "futurelib_innards.h"
#include "qthread_innards.h"

/* GLOBAL DATA (copy everywhere) */
static qthread_shepherd_id_t num_locs = 0;
pthread_key_t future_bookkeeping;

/* this doesn't *need* to be global */
static qthread_shepherd_id_t shep_for_new_futures = 0;
static pthread_mutex_t sfnf_lock;

/* This function is critical to futurelib, and as such must be as fast as
 * possible.
 *
 * If the qthread is not a future, it returns NULL; otherwise, it returns
 * a pointer to the bookkeeping structure associated with that future's
 * shepherd. */
inline location_t *ft_loc(qthread_t * qthr)
{
    return qthread_isfuture(qthr) ? (location_t *)
	pthread_getspecific(future_bookkeeping) : NULL;
}

#ifdef CLEANUP
/* this requires that qthreads haven't been finalized yet */
aligned_t future_shep_cleanup(qthread_t * me, void *arg)
{
    location_t *ptr = (location_t *) pthread_getspecific(future_bookkeeping);

    if (ptr != NULL) {
	pthread_setspecific(future_bookkeeping, NULL);
	pthread_mutex_destroy(&(ptr->vp_count_lock));
	FREE(ptr);
    }
}

void future_cleanup(void)
{
    int i;
    aligned_t *rets;

    rets = (aligned_t *) MALLOC(sizeof(aligned_t) * num_locs);
    for (i = 0; i < num_locs; i++) {
	qthread_fork_to(future_shep_cleanup, NULL, rets + i, i);
    }
    for (i = 0; i < num_locs; i++) {
	qthread_readFF(NULL, rets + i, rets + i);
    }
    FREE(rets);
    pthread_mutex_destroy(&sfnf_lock);
    pthread_key_delete(&future_bookkeeping);
}
#endif

/* this function is used as a qthread; it is run by each shepherd so that each
 * shepherd will get some thread-local data associated with it. This works
 * better in the case of big machines (like massive SMP's) with intelligent
 * pthreads implementations than on PIM, but that's mostly because PIM's libc
 * doesn't support PIM-local data (yet). Better PIM support is coming. */
aligned_t future_shep_init(qthread_t * me, void *arg)
{
    location_t *ptr = (location_t *) pthread_getspecific(future_bookkeeping);

    if (ptr == NULL) {
	ptr = (location_t *) MALLOC(sizeof(location_t));
	ptr->vp_count = 0;
	ptr->vp_max = (unsigned int)arg;
	ptr->id = qthread_shep(me);
	pthread_mutex_init(&(ptr->vp_count_lock), NULL);
	// vp_count is *always* locked. This establishes the waiting queue.
	qthread_lock(me, &(ptr->vp_count));

	pthread_setspecific(future_bookkeeping, ptr);
    } else {
	abort();
    }
    return 0;
}

void future_init(qthread_t * me, int vp_per_loc, int loc_count)
{
    int i;
    aligned_t *rets;

    pthread_mutex_init(&sfnf_lock, NULL);
    pthread_key_create(&future_bookkeeping, NULL);
    num_locs = loc_count;
    rets = (aligned_t *) MALLOC(sizeof(aligned_t) * loc_count);
    for (i = 0; i < loc_count; i++) {
	qthread_fork_to(future_shep_init, (void *)vp_per_loc, rets + i, i);
    }
    for (i = 0; i < loc_count; i++) {
	qthread_readFF(me, rets + i, rets + i);
    }
    FREE(rets);
#ifdef CLEANUP
    atexit(future_cleanup);
#endif
}

/* This is the heart and soul of the futurelib. This function has two purposes:
 * first, it checks for (and grabs, if it exists) an available thread-execution
 * slot. second, if there is no available slot, it adds itself to the waiter
 * queue to get one.
 *
 * NOTE: Because this function is ONLY ever run by the shepherd whose loc we
 * are mucking with, no mutexes are needed to protect data. The only locking
 * required is for waiting when there are too many futures. */
void blocking_vp_incr(qthread_t * me, location_t * loc)
{
#ifdef __PIM__
    pthread_mutex_lock(&(loc->vp_count_lock));
#endif
    DBprintf("Thread %p try blocking increment on loc %d vps %d\n", me,
	     loc->id, loc->vp_count);

    while (loc->vp_count >= loc->vp_max) {
#ifdef __PIM__
	pthread_mutex_unlock(&(loc->vp_count_lock));
#endif
	qthread_lock(me, &(loc->vp_count));
#ifdef __PIM__
	pthread_mutex_lock(&(loc->vp_count_lock));
#endif
    }
    (loc->vp_count)++;
    DBprintf("Thread %p incr loc %d to %d vps\n", me, loc->id, loc->vp_count);
#ifdef __PIM__
    pthread_mutex_unlock(&(loc->vp_count_lock));
#endif
}

/* creates a qthread, on a location defined by the qthread library, and
 * spawns it when the # of futures on that location is below the specified
 * threshold. Thus, this function has two goals:
 * 1. create the thread (aka. "future")
 * 2. bookkeeping to ensure that the location doesn't have too many threads
 */
void future_create(qthread_t * me, aligned_t(*fptr) (qthread_t *, void *),
		   void *arg, aligned_t * retval)
{
    qthread_shepherd_id_t rr;

    pthread_mutex_lock(&sfnf_lock); {
	rr = shep_for_new_futures;
	if (shep_for_new_futures + 1 < num_locs) {
	    ++shep_for_new_futures;
	} else {
	    shep_for_new_futures = 0;
	}
    }
    pthread_mutex_unlock(&sfnf_lock);
    qthread_fork_future_to(fptr, arg, retval, rr);
}

/* This says: "I do not count toward future resource limits, temporarily." */
int future_yield(qthread_t * me)
{
    location_t *loc = ft_loc(me);

    DBprintf("Thread %p yield on loc %p\n", me, loc);
    //Non-futures do not have a vproc to yield
    if (loc != NULL) {
	//yield vproc
	DBprintf("Thread %p yield loc %d vps %d\n", me, loc->id,
		 loc->vp_count);
	pthread_mutex_lock(&(loc->vp_count_lock));
	(loc->vp_count)--;
	pthread_mutex_unlock(&(loc->vp_count_lock));
	qthread_unlock(me, &(loc->vp_count));
	return 1;
    }
    return 0;
}

/* This says: "I count as a future again.", or, more accurately:
 * "I am now a thread that should be limited by the resource limitations."
 */
void future_acquire(qthread_t * me)
{
    location_t *loc = ft_loc(me);

    DBprintf("Thread %p acquire on loc %p\n", me, loc);
    //Non-futures need not acquire a v proc
    if (loc != NULL) {
	blocking_vp_incr(me, loc);
    }
}

/* this is pretty obvious: wait for a thread to finish (ft is supposed
 * to be a thread/future's return value. */
void future_join(qthread_t * me, aligned_t * ft)
{
    DBprintf("Qthread %p join to future %p\n", me, ft);
    qthread_readFF(me, ft, ft);
}

/* this makes me not a future. Once a future exits, the thread may not
 * terminate, but there's no way for it to become a future again. */
void future_exit(qthread_t * me)
{
    DBprintf("Thread %p exit on loc %d\n", me, qthread_shep(me));
    future_yield(me);
    qthread_assertnotfuture(me);
}

/* a more fun version of future_join */
void future_join_all(qthread_t * qthr, aligned_t * fta, int ftc)
{
    int i;

    DBprintf("Qthread %p join all to %d futures\n", qthr, ftc);
    for (i = 0; i < ftc; i++)
	future_join(qthr, fta + i);
}
