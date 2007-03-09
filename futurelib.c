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

#define INIT_LOCK(qqq,xxx) qthread_unlock(qqq, (void*)(xxx))
#define LOCK(qqq,xxx) qthread_lock(qqq, (void*)(xxx))
#define UNLOCK(qqq,xxx) qthread_unlock(qqq, (void*)(xxx))

#define MALLOC(sss) malloc(sss)
#define FREE(sss) free(sss)

typedef struct location_s location_t;

struct location_s
{
    aligned_t vp_count;
    aligned_t vp_sleep;
    pthread_mutex_t waiting_futures;
    unsigned int vp_max;
    unsigned int id;
};

static qthread_shepherd_id_t num_locs = 0;
static location_t **all_locs;
static qthread_shepherd_id_t shep_for_new_futures = 0;
static pthread_mutex_t sfnf_lock;

/* These are our own private stash of secret qthread functions. */
unsigned int qthread_isfuture(const qthread_t * t);
void qthread_assertfuture(qthread_t * t);
void qthread_assertnotfuture(qthread_t * t);
void qthread_fork_future_to(const qthread_f f, const void *arg,
			    aligned_t * ret,
			    const qthread_shepherd_id_t shepherd);

/* This function is critical to futurelib, and as such must be as fast as
 * possible.
 *
 * If the qthread is not a future, it returns NULL; otherwise, it returns
 * a pointer to the bookkeeping structure associated with that future's
 * shepherd. */
inline location_t *ft_loc(qthread_t * qthr)
{
    return qthread_isfuture(qthr) ? all_locs[qthread_shep(qthr)] : NULL;
}

#ifdef CLEANUP
void future_cleanup(void)
{
    unsigned int i;

    for (i = 0; i < num_locs; i++) {
	free(all_locs[i]);
    }
    free(all_locs);
}
#endif

#if 0
pthread_key_t future_bookkeeping;

aligned_t future_shep_init(qthread_t * me, void *arg)
{
    location_t *ptr = (location_t *) pthread_getspecific(future_bookkeeping);

    if (ptr == NULL) {
	ptr = (location_t *) MALLOC(sizeof(location_t));
	ptr->vp_count = 0;
	ptr->vp_max = (unsigned int)arg;
	ptr->id = qthread_shep(me);
	pthread_mutex_init(&(ptr->waiting_futures), NULL);
	INIT_LOCK(me, &(ptr->vp_count));
	INIT_LOCK(me, &(ptr->vp_sleep));
	pthread_setspecific(future_bookkeeping, ptr);
    } else {
	abort();
    }
    return 0;
}
#endif

void future_init(qthread_t * me, int vp_per_loc, int loc_count)
{
    int i;
    aligned_t *rets;

    pthread_mutex_init(&sfnf_lock, NULL);
#if 0
    pthread_key_create(&future_bookkeeping, NULL);
#endif
    num_locs = loc_count;
    all_locs = (location_t **) MALLOC(sizeof(location_t *) * loc_count);
    rets = (aligned_t *) MALLOC(sizeof(aligned_t) * loc_count);
    for (i = 0; i < loc_count; i++) {
#if 0
	qthread_fork_to(future_shep_init, (void *)vp_per_loc, rets + i, i);
#endif
	all_locs[i] = (location_t *) MALLOC(sizeof(location_t));
	all_locs[i]->vp_count = 0;
	all_locs[i]->vp_max = vp_per_loc;
	all_locs[i]->id = i;
	INIT_LOCK(me, &(all_locs[i]->vp_count));
	INIT_LOCK(me, &(all_locs[i]->vp_sleep));
    }
    for (i = 0; i < loc_count; i++) {
	qthread_readFF(me, rets + i, rets + i);
    }
#ifdef CLEANUP
    atexit(future_cleanup);
#endif
}

/* This is the heart and soul of the futurelib. This function has two purposes:
 * first, it checks for (and grabs, if it exists) an available thread-execution
 * slot. second, if there is no available slot, it adds itself to the waiter
 * queue to get one. */
inline void blocking_vp_incr(qthread_t * me, location_t * loc)
{
    DBprintf("Thread %p try blocking increment on loc %d vps %d\n", me,
	     loc->id, loc->vp_count);

    while (1) {
	LOCK(me, &(loc->vp_count));
	if (loc->vp_count >= loc->vp_max) {
	    UNLOCK(me, &(loc->vp_count));
	    LOCK(me, &(loc->vp_sleep));
	} else {
	    (loc->vp_count)++;
	    DBprintf("Thread %p incr loc %d to %d vps\n", me, loc->id,
		     loc->vp_count);
	    UNLOCK(me, &(loc->vp_count));
	    return;
	}
    }
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
    qthread_t *new_thr;
    qthread_shepherd_id_t rr;
    location_t *loc;

    pthread_mutex_lock(&sfnf_lock); {
	rr = shep_for_new_futures;
	if (shep_for_new_futures + 1 < num_locs) {
	    ++shep_for_new_futures;
	} else {
	    shep_for_new_futures = 0;
	}
    }
    pthread_mutex_unlock(&sfnf_lock);

    loc = all_locs[rr];

    DBprintf("Try add future on rr %d loc %d v procs %d\n", rr, loc->id,
	     loc->vp_count);

    blocking_vp_incr(me, loc);
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
	LOCK(me, &(loc->vp_count));
	(loc->vp_count)--;
	UNLOCK(me, &(loc->vp_count));
	UNLOCK(me, &(loc->vp_sleep));
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

/* this makes me not a future. Once exited, there's no way to become a future
 * again. */
void future_exit(qthread_t * me)
{
#if 0
    if (ft_loc(me) == NULL) {
	perror("Null loc qthread try to exit");
	abort();
    }
#endif

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
