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

#define THREAD_LOC(qqq) qthread_shep(qqq)

#define MALLOC(sss) malloc(sss)
#define FREE(sss) free(sss)

typedef struct location_s location_t;

struct location_s
{
    volatile int vp_count;
    unsigned vp_max;
    unsigned vp_sleep;
    int id;
};

location_t **all_locs;

cp_hashtable *qtd_loc;

void init_loc_hash()
{
    if ((qtd_loc =
	 cp_hashtable_create(4, cp_hash_addr,
			     cp_hash_compare_addr)) == NULL) {
	perror("init future hash");
	abort();
    }
}

/* 
   Unless there is an error, multiple threads will not
   simultaneously try to access a map entry for a particular future

   -map_to is used once when the future computation is initiated
   -unmap is used once when the future computation exits
   -ft_loc is used for acquire, yield, and exit and will only
   look up the location of the calling thread

   These basically fulfill a function something like:
   future_self_loc(qthread_self()) which returns NULL 
   if the qthread is was not created as a future
*/

void unmap_from_loc(future_t * ft)
{
    DBprintf("Unmap future %p from loc\n", ft);
    cp_hashtable_remove(qtd_loc, ft);
}

void map_to_loc(future_t * ft, location_t * loc)
{
#if 1
    future_t *old_loc = cp_hashtable_get(qtd_loc, ft);

    if (old_loc != NULL) {
	perror("Warning! future already mapped to loc");
    }
#endif
    DBprintf("Map future %p to loc %d\n", ft, loc->id);
    cp_hashtable_put(qtd_loc, ft, loc);
}

location_t *ft_loc(future_t * ft)
{
    return (location_t *) cp_hashtable_get(qtd_loc, ft);
}

void future_init(qthread_t * qthr, int vp_per_loc, int loc_count)
{
    int i;

    init_loc_hash();
    all_locs = (location_t **) MALLOC(sizeof(location_t *) * loc_count);
    for (i = 0; i < loc_count; i++) {
	all_locs[i] = MALLOC(sizeof(location_t));
	all_locs[i]->vp_count = 0;
	all_locs[i]->vp_max = vp_per_loc;
	all_locs[i]->id = i;
	INIT_LOCK(qthr, &(all_locs[i]->vp_count));
	INIT_LOCK(qthr, &(all_locs[i]->vp_sleep));
    }
}

inline void blocking_vp_incr(qthread_t * qthr, location_t * loc)
{
    DBprintf("Thread %p try blocking increment on loc %d vps %d\n", qthr,
	     loc->id, loc->vp_count);

    while (1) {
	LOCK(qthr, &(loc->vp_count));
	if (loc->vp_count >= loc->vp_max) {
	    UNLOCK(qthr, &(loc->vp_count));
	    LOCK(qthr, &(loc->vp_sleep));
	} else {
	    (loc->vp_count)++;
	    DBprintf("Thread %p incr loc %d to %d vps\n", qthr, loc->id,
		     loc->vp_count);
	    UNLOCK(qthr, &(loc->vp_count));
	    return;
	}
    }
}

void future_create(qthread_t * qthr, aligned_t (*fptr) (qthread_t *, void *),
			void *arg, aligned_t * retval)
{
    qthread_t *new_thr = (qthread_t *) qthread_prepare(fptr, arg, retval);

    int rr = THREAD_LOC(new_thr);
    location_t *loc = all_locs[rr];

    DBprintf("Try add future on rr %d loc %d v procs %d\n", rr, loc->id,
	     loc->vp_count);

    blocking_vp_incr(qthr, loc);
    map_to_loc(new_thr, loc);
    qthread_schedule(new_thr);
}

int future_yield(qthread_t * qthr)
{
    location_t *loc = ft_loc(qthr);

    DBprintf("Thread %p yield on loc %p\n", qthr, loc);
    //Non-futures do not have a vproc to yield
    if (loc != NULL) {
	//yield vproc
	DBprintf("Thread %p yield loc %d vps %d\n", qthr, loc->id,
		 loc->vp_count);
	LOCK(qthr, &(loc->vp_count));
	(loc->vp_count)--;
	UNLOCK(qthr, &(loc->vp_count));
	UNLOCK(qthr, &(loc->vp_sleep));
	return 1;
    }
    return 0;
}

void future_acquire(qthread_t * qthr)
{
    location_t *loc = ft_loc(qthr);

    DBprintf("Thread %p acquire on loc %p\n", qthr, loc);
    //Non-futures need not acquire a v proc
    if (loc != NULL) {
	blocking_vp_incr(qthr, loc);
    }
}

void future_join(qthread_t * qthr, aligned_t * ft)
{
    DBprintf("Qthread %p join to future %p\n", qthr, ft);
    //qthread_join(qthr, ft);
    qthread_readFF(qthr, ft, ft);
}

void future_exit(qthread_t * qthr)
{
#if 1
    if (ft_loc(qthr) == NULL) {
	perror("Null loc qthread try to exit");
	abort();
    }
#endif

    DBprintf("Thread %p exit on loc %d\n", qthr, THREAD_LOC(qthr));
    future_yield(qthr);
    unmap_from_loc(qthr);
}

void future_join_all(qthread_t * qthr, aligned_t * fta, int ftc)
{
    int i;

    DBprintf("Qthread %p join all to %d futures\n", qthr, ftc);
    for (i = 0; i < ftc; i++)
	future_join(qthr, fta+i);
}
