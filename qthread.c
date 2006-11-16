#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>		       /* for malloc() and abort() */
#include <assert.h>		       /* for assert() */
#if defined(HAVE_UCONTEXT_H) && defined(HAVE_CONTEXT_FUNCS)
# include <ucontext.h>		       /* for make/get/swap-context functions */
#else
# include "taskimpl.h"
#endif
#include <stdarg.h>		       /* for va_start and va_end */
#include <stdint.h>		       /* for UINT8_MAX */
#include <string.h>		       /* for memset() */
#if !HAVE_MEMCPY
# define memcpy(d, s, n) bcopy((s), (d), (n))
# define memmove(d, s, n) bcopy((s), (d), (n))
#endif
#ifdef NEED_RLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif

#include <cprops/mempool.h>
#include <cprops/hashtable.h>
#include <cprops/linked_list.h>
#include "qthread.h"

#ifndef UINT8_MAX
#define UINT8_MAX (255)
#endif

#define MACHINEMASK (~(WORDSIZE-1))

/* If __USE_FILE_OFFSET64, and NEED_RLIMIT, and we don't have __REDIRECT, we
 * #define rlimit to rlimit64, and its prototype returns a 'struct rlimit64 *',
 * but the user's code expects to be able to designate it by 'struct rlimit *'.
 */
#if defined(__USE_FILE_OFFSET64) && defined(NEED_RLIMIT) && ! defined(__REDIRECT)
# define rlimit rlimit64
#endif

#ifdef QTHREAD_DEBUG
/* for the vprintf in qthread_debug() */
/* 8MB stack */
/* unless you're doing some limit testing with very small stacks, the stack
 * size MUST be a multiple of the page size */
# define QTHREAD_DEFAULT_STACK_SIZE 4096*2048
#else
# ifdef REALLY_SMALL_STACKS
#  define QTHREAD_DEFAULT_STACK_SIZE 2048
# else
#  define QTHREAD_DEFAULT_STACK_SIZE 4096
# endif
#endif

/* internal constants */
#define QTHREAD_STATE_NEW               0
#define QTHREAD_STATE_RUNNING           1
#define QTHREAD_STATE_YIELDED           2
#define QTHREAD_STATE_BLOCKED           3
#define QTHREAD_STATE_FEB_BLOCKED       4
#define QTHREAD_STATE_TERMINATED        5
#define QTHREAD_STATE_DONE              6
#define QTHREAD_STATE_TERM_SHEP         UINT8_MAX

#ifndef UNPOOLED_QTHREAD_T
#define ALLOC_QTHREAD(shep)             (qthread_t *) cp_mempool_alloc(shep.qthread_pool)
#define FREE_QTHREAD(shep, t) cp_mempool_free(shep->qthread_pool, t)
#else
#define ALLOC_QTHREAD(shep)             (qthread_t *) malloc(sizeof(qthread_t))
#define FREE_QTHREAD(shep, t) free(t)
#endif

#ifndef UNPOOLED_STACKS
#define ALLOC_STACK(shep) cp_mempool_alloc(qlib->kthreads[shep].stack_pool)
#define FREE_STACK(shep, t) cp_mempool_free(shep->stack_pool, t)
#else
#define ALLOC_STACK(shep) malloc(stack_size)
#define FREE_STACK(shep, t) free(t)
#endif

#ifndef UNPOOLED_CONTEXTS
#define ALLOC_CONTEXT(shep) (ucontext_t *) cp_mempool_alloc(qlib->kthreads[shep].context_pool)
#define FREE_CONTEXT(shep, t) cp_mempool_free(shep->context_pool, t)
#else
#define ALLOC_CONTEXT(shep) (ucontext_t *) malloc(sizeof(ucontext_t))
#define FREE_CONTEXT(shep, t) free(t)
#endif

#ifndef UNPOOLED_QUEUES
#define ALLOC_QUEUE(shep) (qthread_queue_t *) cp_mempool_alloc(qlib->kthreads[shep].queue_pool)
#define FREE_QUEUE(shep, t) cp_mempool_free(qlib->kthreads[shep].queue_pool, t)
#else
#define ALLOC_QUEUE(shep) (qthread_queue_t *) malloc(sizeof(qthread_queue_t))
#define FREE_QUEUE(shep, t) free(t)
#endif

#ifndef UNPOOLED_LOCKS
#define ALLOC_LOCK(shep) (qthread_lock_t *) cp_mempool_alloc(qlib->kthreads[shep].lock_pool)
#define FREE_LOCK(shep, t) cp_mempool_free(qlib->kthreads[shep].lock_pool, t)
#else
#define ALLOC_LOCK(shep) (qthread_lock_t *) malloc(sizeof(qthread_lock_t))
#define FREE_LOCK(shep, t) free(t)
#endif

#ifndef UNPOOLED_ADDRRES
#define ALLOC_ADDRRES(shep) (qthread_addrres_t *) cp_mempool_alloc(qlib->kthreads[shep].addrres_pool)
#define FREE_ADDRRES(shep, t) cp_mempool_free(qlib->kthreads[shep].addrres_pool, t)
#else
#define ALLOC_ADDRRES(shep) (qthread_addrres_t *) malloc(sizeof(qthread_addrres_t))
#define FREE_ADDRRES(shep, t) free(t)
#endif

#ifndef UNPOOLED_ADDRSTAT
#define ALLOC_ADDRSTAT(shep) (qthread_addrstat_t *) cp_mempool_alloc(qlib->kthreads[shep].addrstat_pool)
#define FREE_ADDRSTAT(shep, t) cp_mempool_free(qlib->kthreads[shep].addrstat_pool, t)
#else
#define ALLOC_ADDRSTAT(shep) (qthread_addrstat_t *) malloc(sizeof(qthread_addrstat_t))
#define FREE_ADDRSTAT(shep, t) free(t)
#endif

#ifndef UNPOOLED_ADDRSTAT2
#define ALLOC_ADDRSTAT2(shep) (qthread_addrstat2_t *) cp_mempool_alloc(qlib->kthreads[shep].addrstat2_pool)
#define FREE_ADDRSTAT2(shep, t) cp_mempool_free(qlib->kthreads[shep].addrstat2_pool, t)
#else
#define ALLOC_ADDRSTAT2(shep) (qthread_addrstat2_t *) malloc(sizeof(qthread_addrstat2_t))
#define FREE_ADDRSTAT2(shep, t) free(t)
#endif

#define ALIGN(d, s, f) do { \
    s = (aligned_t *) (((size_t) d) & MACHINEMASK); \
    if (s != d) { \
	fprintf(stderr, \
		"WARNING: " f ": unaligned address %p ... assuming %p\n", \
		(void *) d, (void *) s); \
    } \
} while(0)

/* internal data structures */
typedef struct qthread_lock_s qthread_lock_t;
typedef struct qthread_shepherd_s qthread_shepherd_t;
typedef struct qthread_queue_s qthread_queue_t;

struct qthread_s
{
    unsigned int thread_id;
    unsigned char thread_state;
    unsigned int detached:1;

    /* the pthread we run on */
    qthread_shepherd_id_t shepherd;
    /* a pointer used for passing information back to the shepherd when
     * becoming blocked */
    struct qthread_lock_s *blockedon;

    /* the function to call (that defines this thread) */
    qthread_f f;
    void *arg;			/* user defined data */

    ucontext_t *context;	/* the context switch info */
    void *stack;		/* the thread's stack */
    ucontext_t *return_context;	/* context of parent kthread */

    struct qthread_s *next;
};

struct qthread_queue_s
{
    qthread_t *head;
    qthread_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t notempty;
};

struct qthread_shepherd_s
{
    pthread_t kthread;
    unsigned kthread_index;
    qthread_t *current;
    qthread_queue_t *ready;
    cp_mempool *qthread_pool;
    cp_mempool *list_pool;
    cp_mempool *queue_pool;
    cp_mempool *lock_pool;
    cp_mempool *addrres_pool;
    cp_mempool *addrstat_pool;
    cp_mempool *addrstat2_pool;
    cp_mempool *stack_pool;
    cp_mempool *context_pool;
};

struct qthread_lock_s
{
    unsigned owner;
    pthread_mutex_t lock;
    qthread_queue_t *waiting;
};

typedef struct qthread_addrres_s
{
    aligned_t *addr;		/* ptr to the memory NOT being blocked on */
    qthread_t *waiter;
    struct qthread_addrres_s *next;
} qthread_addrres_t;

typedef struct qthread_addrstat_s
{
    pthread_mutex_t lock;
    qthread_addrres_t *EFQ;
    qthread_addrres_t *FEQ;
    qthread_addrres_t *FFQ;
    unsigned int full:1;
} qthread_addrstat_t;

typedef struct qthread_addrstat2_s
{
    qthread_addrstat_t *m;
    aligned_t *addr;
    struct qthread_addrstat2_s *next;
} qthread_addrstat2_t;

typedef struct
{
    int nkthreads;
    struct qthread_shepherd_s *kthreads;

    unsigned qthread_stack_size;
    unsigned master_stack_size;
    unsigned max_stack_size;

    /* assigns a unique thread_id mostly for debugging! */
    unsigned max_thread_id;
    pthread_mutex_t max_thread_id_lock;

    /* round robin scheduler - can probably be smarter */
    unsigned sched_kthread;
    pthread_mutex_t sched_kthread_lock;

    /* this is how we manage FEB-type locks
     * NOTE: this can be a major bottleneck and we should probably create
     * multiple hashtables to improve performance
     */
    cp_hashtable *locks;
    /* these are separated out for memory reasons: if you can get away with
     * simple locks, then you can use less memory. Subject to the same
     * bottleneck concerns as the above hashtable, though these are slightly
     * better at shrinking their critical section. */
    cp_hashtable *FEBs;
} qlib_t;

static cp_hashtable *p_to_shep = NULL;

/* internal globals */
static qlib_t *qlib = NULL;

/* Internal functions */
static void qthread_wrapper(void *arg);

static void qthread_FEBlock_delete(qthread_addrstat_t * m);
static inline qthread_t *qthread_thread_new(const qthread_f f,
					    const void *arg,
					    const qthread_shepherd_id_t
					    shepherd);
static inline void qthread_thread_free(qthread_t * t);
static inline qthread_queue_t *qthread_queue_new(qthread_shepherd_id_t
						 shepherd);
static inline void qthread_queue_free(qthread_queue_t * q,
				      qthread_shepherd_id_t shepherd);
static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t);
static inline qthread_t *qthread_dequeue(qthread_queue_t * q);
static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q);
static inline void qthread_exec(qthread_t * t, ucontext_t * c);
static inline void qthread_back_to_master(qthread_t * t);
static inline void qthread_gotlock_fill(qthread_addrstat_t * m, void *maddr,
					const qthread_shepherd_id_t
					threadshep, const char recursive);
static inline void qthread_gotlock_empty(qthread_addrstat_t * m, void *maddr,
					 const qthread_shepherd_id_t
					 threadshep, const char recursive);

#ifdef QTHREAD_NO_ASSERTS
#define QTHREAD_LOCK(l) pthread_mutex_lock(l)
#define QTHREAD_UNLOCK(l) pthread_mutex_unlock(l)
#define QTHREAD_INITLOCK(l) pthread_mutex_init(l, NULL)
#define QTHREAD_DESTROYLOCK(l) pthread_mutex_destroy(l)
#define QTHREAD_INITCOND(l) pthread_cond_init(l, NULL)
#define QTHREAD_DESTROYCOND(l) pthread_cond_destroy(l)
#define QTHREAD_SIGNAL(l) pthread_cond_signal(l)
#define QTHREAD_CONDWAIT(c, l) pthread_cond_wait(c, l)
#else
#define QTHREAD_LOCK(l) assert(pthread_mutex_lock(l) == 0)
#define QTHREAD_UNLOCK(l) assert(pthread_mutex_unlock(l) == 0)
#define QTHREAD_INITLOCK(l) assert(pthread_mutex_init(l, NULL) == 0)
#define QTHREAD_DESTROYLOCK(l) assert(pthread_mutex_destroy(l) == 0)
#define QTHREAD_INITCOND(l) assert(pthread_cond_init(l, NULL) == 0)
#define QTHREAD_DESTROYCOND(l) assert(pthread_cond_destroy(l) == 0)
#define QTHREAD_SIGNAL(l) assert(pthread_cond_signal(l) == 0)
#define QTHREAD_CONDWAIT(c, l) assert(pthread_cond_wait(c, l) == 0)
#endif

#define ATOMIC_INC(r, x, l) do { \
    QTHREAD_LOCK(l); \
    r = *(x); \
    *(x) += 1; \
    QTHREAD_UNLOCK(l); \
} while (0)

#define ATOMIC_INC_MOD(r, x, l, m) do {\
    QTHREAD_LOCK(l); \
    r = *(x); \
    if (*(x) + 1 < (m)) { \
	*(x) += 1; \
    } else { \
	*(x) = 0; \
    } \
    QTHREAD_UNLOCK(l); \
} while (0)

#if 0				       /* currently not used */
static inline unsigned qthread_internal_atomic_inc(unsigned *x,
						   pthread_mutex_t * lock)
{				       /*{{{ */
    unsigned r;

    QTHREAD_LOCK(lock);
    r = *x;
    *x++;
    QTHREAD_UNLOCK(lock);
    return (r);
}				       /*}}} */

static inline unsigned qthread_internal_atomic_inc_mod(unsigned *x,
						       pthread_mutex_t * lock,
						       int mod)
{				       /*{{{ */
    unsigned r;

    QTHREAD_LOCK(lock);
    r = *x;
    if (*x + 1 < mod) {
	*x++;
    } else {
	*x = 0;
    }
    QTHREAD_UNLOCK(lock);
    return (r);
}				       /*}}} */

static inline unsigned qthread_internal_atomic_check(unsigned *x,
						     pthread_mutex_t * lock)
{				       /*{{{ */
    unsigned r;

    QTHREAD_LOCK(lock);
    r = *x;
    QTHREAD_UNLOCK(lock);

    return (r);
}				       /*}}} */
#endif

/*#define QTHREAD_DEBUG 1*/
/* for debugging */
#ifdef QTHREAD_DEBUG
static inline void qthread_debug(char *format, ...)
{				       /*{{{ */
    static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;
    va_list args;

    QTHREAD_LOCK(&output_lock);

    fprintf(stderr, "qthread_debug(): ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);		       /* KBW: helps keep things straight */

    QTHREAD_UNLOCK(&output_lock);
}				       /*}}} */
#else
#define qthread_debug(...) do{ }while(0)
#endif

/* the qthread_shepherd() is the pthread responsible for actually
 * executing the work units
 *
 * this function is the workhorse of the library: this is the function that
 * gets spawned several times. */
static void *qthread_shepherd(void *arg)
{				       /*{{{ */
    qthread_shepherd_t *me = (qthread_shepherd_t *) arg;	/* rcm -- not used */
    ucontext_t my_context;
    qthread_t *t;
    int done = 0;

    qthread_debug("qthread_shepherd(%u): forked\n", me->kthread_index);

    while (!done) {
	t = qthread_dequeue(me->ready);

	qthread_debug
	    ("qthread_shepherd(%u): dequeued thread id %d/state %d\n",
	     me->kthread_index, t->thread_id, t->thread_state);

	if (t->thread_state == QTHREAD_STATE_TERM_SHEP) {
	    done = 1;
	    qthread_thread_free(t);
	} else {
#ifndef QTHREAD_NO_ASSERTS
	    assert((t->thread_state == QTHREAD_STATE_NEW) ||
		   (t->thread_state == QTHREAD_STATE_RUNNING));

	    assert(t->f != NULL);

	    /* note: there's a good argument that the following should
	     * be: (*t->f)(t), however the state management would be
	     * more complex 
	     */

	    assert(t->shepherd == me->kthread_index);
#endif
	    me->current = t;
	    qthread_exec(t, &my_context);
	    me->current = NULL;
	    qthread_debug("qthread_shepherd(%u): back from qthread_exec\n",
			  me->kthread_index);
	    switch (t->thread_state) {
		case QTHREAD_STATE_YIELDED:	/* reschedule it */
		    qthread_debug
			("qthread_shepherd(%u): rescheduling thread %p\n",
			 me->kthread_index, t);
		    t->thread_state = QTHREAD_STATE_RUNNING;
		    qthread_enqueue(qlib->kthreads[t->shepherd].ready, t);
		    break;

		case QTHREAD_STATE_FEB_BLOCKED:	/* unlock the related FEB address locks, and re-arrange memory to be correct */
		    qthread_debug
			("qthread_shepherd(%u): unlocking FEB address locks of thread %p\n",
			 me->kthread_index, t);
		    t->thread_state = QTHREAD_STATE_BLOCKED;
		    pthread_mutex_unlock(&
					 (((qthread_addrstat_t *) (t->
								   blockedon))->
					  lock));
		    break;

		case QTHREAD_STATE_BLOCKED:	/* put it in the blocked queue */
		    qthread_debug
			("qthread_shepherd(%u): adding blocked thread %p to blocked queue\n",
			 me->kthread_index, t);
		    qthread_enqueue((qthread_queue_t *) t->blockedon->waiting,
				    t);
		    QTHREAD_UNLOCK(&(t->blockedon->lock));
		    break;

		case QTHREAD_STATE_TERMINATED:
		    qthread_debug
			("qthread_shepherd(%u): thread %p is in state terminated.\n",
			 me->kthread_index, t);
		    t->thread_state = QTHREAD_STATE_DONE;
		    if (t->detached == 0) {
			/* we can remove the stack and the context... */
			if (t->context) {
			    FREE_CONTEXT(me, t->context);
			    t->context = NULL;
			}
			if (t->stack != NULL) {
			    FREE_STACK(me, t->stack);
			    t->stack = NULL;
			}
			/* but we rely on the joiner to clean up the structure
			 * itself (why? because there's a qthread_t handle out
			 * there that the user is responsible for) */
			qthread_unlock(t, t);
		    } else {
			qthread_thread_free(t);
		    }
		    break;
	    }
	}
    }

    qthread_debug("qthread_shepherd(%u): finished\n", me->kthread_index);
    pthread_exit(NULL);
    return NULL;
}				       /*}}} */

int qthread_init(const int nkthreads)
{				       /*{{{ */
    int r;
    size_t i;

#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    qthread_debug("qthread_init(): began.\n");

    if ((qlib = (qlib_t *) malloc(sizeof(qlib_t))) == NULL) {
	perror("qthread_init()");
	abort();
    }

    /* initialize the FEB-like locking structures */

    /* this is synchronized with read/write locks by default */
    if ((qlib->locks =
	 cp_hashtable_create(100, cp_hash_addr,
			     cp_hash_compare_addr)) == NULL) {
	perror("qthread_init()");
	abort();
    }
    if ((qlib->FEBs =
	 cp_hashtable_create_by_option(COLLECTION_MODE_DEEP, 100,
				       cp_hash_addr, cp_hash_compare_addr,
				       NULL, NULL, NULL, NULL)) == NULL) {
	perror("qthread_init()");
	abort();
    }

    /* initialize the kernel threads and scheduler */
    qlib->nkthreads = nkthreads;
    if ((qlib->kthreads = (qthread_shepherd_t *)
	 malloc(sizeof(qthread_shepherd_t) * nkthreads)) == NULL) {
	perror("qthread_init()");
	abort();
    }

    qlib->qthread_stack_size = QTHREAD_DEFAULT_STACK_SIZE;
    qlib->sched_kthread = 0;
    qlib->max_thread_id = 0;
    QTHREAD_INITLOCK(&qlib->sched_kthread_lock);
    QTHREAD_INITLOCK(&qlib->max_thread_id_lock);

#ifdef NEED_RLIMIT
    assert(getrlimit(RLIMIT_STACK, &rlp) == 0);
    qthread_debug("stack sizes ... cur: %u max: %u\n", rlp.rlim_cur,
		  rlp.rlim_max);
    qlib->master_stack_size = rlp.rlim_cur;
    qlib->max_stack_size = rlp.rlim_max;
#endif

    /* set up the lookup table */
    p_to_shep =
	cp_hashtable_create_by_mode(COLLECTION_MODE_NOSYNC, nkthreads,
				    cp_hash_addr, cp_hash_compare_addr);

    /* set up the memory pools */
    for (i = 0; i < nkthreads; i++) {
	/* the first three pools (qthread_pool, stack_pool, and context_pool)
	 * must be synchronized, because forking is done by the main thread as
	 * well. however, by putting one on each pthread, there's less
	 * contention for those locks. */
	qlib->kthreads[i].qthread_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_t), 1000);
	/* this prevents an alignment problem. With most mallocs there's
	 * actually no difference in memory consumption between these two sizes
	 */
	if (qlib->qthread_stack_size < 4096) {
	    qlib->kthreads[i].stack_pool =
		cp_mempool_create_by_option(0, 4096, 4096 * 1000);
	} else {
	    qlib->kthreads[i].stack_pool =
		cp_mempool_create_by_option(0, qlib->qthread_stack_size,
					    qlib->qthread_stack_size * 1000);
	}
	/* this prevents an alignment problem. */
	if (sizeof(ucontext_t) < 2048) {
	    qlib->kthreads[i].context_pool =
		cp_mempool_create_by_option(0, 2048, 2048 * 1000);
	} else {
	    qlib->kthreads[i].context_pool =
		cp_mempool_create_by_option(0, sizeof(ucontext_t),
					    sizeof(ucontext_t) * 1000);
	}

	/* the following SHOULD only be accessed by one thread at a time, so
	 * should be quite safe unsynchronized. If things fail, though...
	 * resynchronize them and see if that fixes it. */
	qlib->kthreads[i].list_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(cp_list_entry), 0);
	qlib->kthreads[i].queue_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(qthread_queue_t), 0);
	qlib->kthreads[i].lock_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(qthread_lock_t), 0);
	qlib->kthreads[i].addrres_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(qthread_addrres_t), 0);
	qlib->kthreads[i].addrstat_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(qthread_addrstat_t), 0);
	qlib->kthreads[i].addrstat2_pool =
	    cp_mempool_create_by_option(COLLECTION_MODE_NOSYNC,
					sizeof(qthread_addrstat2_t), 0);
    }

    /* spawn the number of shepherd threads that were specified */
    for (i = 0; i < nkthreads; i++) {
	qlib->kthreads[i].kthread_index = i;
	qlib->kthreads[i].ready =
	    qthread_queue_new((qthread_shepherd_id_t) - 1);

	qthread_debug("qthread_init(): forking shepherd thread %p\n",
		      &qlib->kthreads[i]);

	if ((r =
	     pthread_create(&qlib->kthreads[i].kthread, NULL,
			    qthread_shepherd, &qlib->kthreads[i])) != 0) {
	    fprintf(stderr, "qthread_init: pthread_create() failed (%d)\n",
		    r);
	    abort();
	}
	cp_hashtable_put(p_to_shep, (void *)(qlib->kthreads[i].kthread),
			 (void *)(i + 1));
    }

    qthread_debug("qthread_init(): finished.\n");
    return 0;
}				       /*}}} */

void qthread_finalize(void)
{				       /*{{{ */
    int i, r;
    qthread_t *t;

#ifndef QTHREAD_NO_ASSERTS
    assert(qlib != NULL);
#endif

    qthread_debug("qthread_finalize(): began.\n");

    /* rcm - probably need to put a "turn off the library flag" here, but,
     * the programmer can ensure that no further threads are forked for now
     */

    /* enqueue the termination thread sentinal */
    for (i = 0; i < qlib->nkthreads; i++) {
	t = qthread_thread_new(NULL, NULL, i);
	t->thread_state = QTHREAD_STATE_TERM_SHEP;
	t->thread_id = (unsigned int)-1;
	qthread_enqueue(qlib->kthreads[i].ready, t);
    }

    /* wait for each thread to drain it's queue! */
    for (i = 0; i < qlib->nkthreads; i++) {
	if ((r = pthread_join(qlib->kthreads[i].kthread, NULL)) != 0) {
	    fprintf(stderr, "qthread_finalize: pthread_join() failed (%d)\n",
		    r);
	    abort();
	}
	qthread_queue_free(qlib->kthreads[i].ready,
			   (qthread_shepherd_id_t) - 1);
    }

    cp_hashtable_destroy(qlib->locks);
    cp_hashtable_destroy_custom(qlib->FEBs, NULL, (cp_destructor_fn)
				qthread_FEBlock_delete);
    cp_hashtable_destroy(p_to_shep);

    QTHREAD_DESTROYLOCK(&qlib->sched_kthread_lock);
    QTHREAD_DESTROYLOCK(&qlib->max_thread_id_lock);

    for (i = 0; i < qlib->nkthreads; ++i) {
	cp_mempool_destroy(qlib->kthreads[i].qthread_pool);
	cp_mempool_destroy(qlib->kthreads[i].list_pool);
	cp_mempool_destroy(qlib->kthreads[i].queue_pool);
	cp_mempool_destroy(qlib->kthreads[i].lock_pool);
	cp_mempool_destroy(qlib->kthreads[i].addrres_pool);
	cp_mempool_destroy(qlib->kthreads[i].addrstat_pool);
	cp_mempool_destroy(qlib->kthreads[i].addrstat2_pool);
	cp_mempool_destroy(qlib->kthreads[i].stack_pool);
	cp_mempool_destroy(qlib->kthreads[i].context_pool);
    }
    free(qlib->kthreads);
    free(qlib);
    qlib = NULL;

    qthread_debug("qthread_finalize(): finished.\n");
}				       /*}}} */

qthread_t *qthread_self(void)
{				       /*{{{ */
    void *ret;

    /*size_t mask; */
    qthread_t *t;

    ret = cp_hashtable_get(p_to_shep, (void *)pthread_self());
    if (ret == NULL) {
	return NULL;
    }
#if 0
    printf("stack size: %lu\n", qlib->qthread_stack_size);
    printf("ret is at %p\n", &ret);
    mask = qlib->qthread_stack_size - 1;	/* assuming the stack is a power of two */
    printf("mask is: %p\n", ((size_t) (qlib->qthread_stack_size) - 1));
    printf("low order bits: 0x%lx\n",
	   ((size_t) (&ret) % (size_t) (qlib->qthread_stack_size)));
    printf("low order bits: 0x%lx\n", (size_t) (&ret) & mask);
    printf("calc stack pointer is: %p\n", ((size_t) (&ret) & ~mask));
    printf("top is then: 0x%lx\n",
	   ((size_t) (&ret) & ~mask) + qlib->qthread_stack_size);
#endif
    /* see this double-cast? yeah, that's because gcc is wonky that way. ret is
     * a pointer, and gcc doesn't want to cast it directly to something smaller
     * (like an unsigned char). */
    t = qlib->kthreads[((qthread_shepherd_id_t) (size_t) ret) - 1].current;
    /* printf("stack pointer should be %p\n", t->stack); */
    return t;
}				       /*}}} */

/************************************************************/
/* functions to manage thread stack allocation/deallocation */
/************************************************************/

static inline qthread_t *qthread_thread_bare(const qthread_f f,
					     const void *arg,
					     const qthread_shepherd_id_t
					     shepherd)
{				       /*{{{ */
    qthread_t *t;

    t = ALLOC_QTHREAD((qlib->kthreads[shepherd]));
    if (t == NULL) {
	perror("qthread_prepare()");
	abort();
    }
    /* give the thread an ID number */
    ATOMIC_INC(t->thread_id, &qlib->max_thread_id, &qlib->max_thread_id_lock);
    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = (void *)arg;
    t->blockedon = NULL;
    t->shepherd = shepherd;

    return t;
}				       /*}}} */

static inline void qthread_thread_plush(qthread_t * t,
					const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    ucontext_t *uc;
    void *stack;

    uc = ALLOC_CONTEXT(shepherd);
    stack = ALLOC_STACK(shepherd);

    if (uc == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->context = uc;
    if (stack == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->stack = stack;
}				       /*}}} */

/* this could be reduced to a qthread_thread_bare() and qthread_thread_plush(),
 * but I *think* doing it this way makes it faster. maybe not, I haven't tested
 * it. */
static inline qthread_t *qthread_thread_new(const qthread_f f,
					    const void *arg,
					    const qthread_shepherd_id_t
					    shepherd)
{				       /*{{{ */
    qthread_t *t;
    ucontext_t *uc;
    void *stack;

    t = ALLOC_QTHREAD((qlib->kthreads[shepherd]));
    uc = ALLOC_CONTEXT(shepherd);
    stack = ALLOC_STACK(shepherd);

    if (t == NULL) {
	perror("qthread_thread_new()");
	abort();
    }

    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = (void *)arg;
    t->blockedon = NULL;
    t->shepherd = shepherd;

    /* re-ordered the checks to help optimization */
    if (uc == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->context = uc;
    if (stack == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->stack = stack;

    /* give the thread an ID number */
    ATOMIC_INC(t->thread_id, &qlib->max_thread_id, &qlib->max_thread_id_lock);

    return (t);
}				       /*}}} */

static inline void qthread_thread_free(qthread_t * t)
{				       /*{{{ */
    qthread_shepherd_t *shep;

#ifndef QTHREAD_NO_ASSERTS
    assert(t != NULL);
#endif

    shep = &(qlib->kthreads[t->shepherd]);
    if (t->context) {
	FREE_CONTEXT(shep, t->context);
    }
    if (t->stack != NULL) {
	FREE_STACK(shep, t->stack);
    }
    FREE_QTHREAD(shep, t);
}				       /*}}} */


/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

static inline qthread_queue_t *qthread_queue_new(qthread_shepherd_id_t
						 shepherd)
{				       /*{{{ */
    qthread_queue_t *q;

    if (shepherd == (qthread_shepherd_id_t) - 1) {
	q = (qthread_queue_t *) malloc(sizeof(qthread_queue_t));
    } else {
	q = ALLOC_QUEUE(shepherd);
    }
    if (q == NULL) {
	perror("qthread_queue_new()");
	abort();
    }

    q->head = NULL;
    q->tail = NULL;
    QTHREAD_INITLOCK(&q->lock);
    QTHREAD_INITCOND(&q->notempty);
    return (q);
}				       /*}}} */

static inline void qthread_queue_free(qthread_queue_t * q,
				      qthread_shepherd_id_t shepherd)
{				       /*{{{ */
#ifndef QTHREAD_NO_ASSERTS
    assert((q->head == NULL) && (q->tail == NULL));
#endif
    QTHREAD_DESTROYLOCK(&q->lock);
    QTHREAD_DESTROYCOND(&q->notempty);
    if (shepherd == (qthread_shepherd_id_t) - 1) {
	free(q);
    } else {
	FREE_QUEUE(shepherd, q);
    }
}				       /*}}} */

static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t)
{				       /*{{{ */
#ifndef QTHREAD_NO_ASSERTS
    assert(t != NULL);
    assert(q != NULL);
#endif

    qthread_debug("qthread_enqueue(%p,%p): started\n", q, t);

    QTHREAD_LOCK(&q->lock);

    t->next = NULL;

    if (q->head == NULL) {	       /* surely then tail is also null; no need to check */
	q->head = t;
	q->tail = t;
	QTHREAD_SIGNAL(&q->notempty);
    } else {
	q->tail->next = t;
	q->tail = t;
    }

    qthread_debug("qthread_enqueue(%p,%p): finished\n", q, t);
    QTHREAD_UNLOCK(&q->lock);
}				       /*}}} */

static inline qthread_t *qthread_dequeue(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t;

    qthread_debug("qthread_dequeue(%p): started\n", q);

    QTHREAD_LOCK(&q->lock);

    while (q->head == NULL) {	       /* if head is null, then surely tail is also null */
	QTHREAD_CONDWAIT(&q->notempty, &q->lock);
    }

#ifndef QTHREAD_NO_ASSERTS
    assert(q->head != NULL);
#endif

    t = q->head;
    if (q->head != q->tail) {
	q->head = q->head->next;
    } else {
	q->head = NULL;
	q->tail = NULL;
    }
    t->next = NULL;

    QTHREAD_UNLOCK(&q->lock);

    qthread_debug("qthread_dequeue(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */

static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t = NULL;

    /* NOTE: it's up to the caller to lock/unlock the queue! */
    qthread_debug("qthread_dequeue_nonblocking(%p,%p): started\n", q, t);

    if (q->head == NULL) {
	qthread_debug
	    ("qthread_dequeue_nonblocking(%p,%p): finished (nobody in list)\n",
	     q, t);
	return (NULL);
    }

    t = q->head;
    if (q->head != q->tail) {
	q->head = q->head->next;
    } else {
	q->head = NULL;
	q->tail = NULL;
    }
    t->next = NULL;

    qthread_debug("qthread_dequeue_nonblocking(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */

/* this function is for maintenance of the FEB hashtables. SHOULD only be
 * necessary for things left over when qthread_finalize is called */
static void qthread_FEBlock_delete(qthread_addrstat_t * m)
{				       /*{{{ */
    /* NOTE! This is only safe if this function (as part of destroying the FEB
     * hash table) is ONLY called once all other pthreads have been joined */
    FREE_ADDRSTAT(0, m);
}				       /*}}} */

/* this function runs a thread until it completes or yields */
static void qthread_wrapper(void *arg)
{				       /*{{{ */
    qthread_t *t = (qthread_t *) arg;

    qthread_debug("qthread_wrapper(): executing f=%p arg=%p.\n", t->f,
		  t->arg);
    (t->f) (t);
    t->thread_state = QTHREAD_STATE_TERMINATED;

    qthread_debug("qthread_wrapper(): f=%p arg=%p completed.\n", t->f,
		  t->arg);
#if !defined(HAVE_CONTEXT_FUNCS) || defined(NEED_RLIMIT)
    /* without a built-in make/get/swapcontext, we're relying on the portable
     * one in context.c (stolen from libtask). unfortunately, this home-made
     * context stuff does not allow us to set up a uc_link pointer that will be
     * returned to once qthread_wrapper returns, so we have to do it by hand.
     *
     * We also have to do it by hand if the context switch requires a
     * stack-size modification.
     */
    qthread_back_to_master(t);
#endif
}				       /*}}} */

static inline void qthread_exec(qthread_t * t, ucontext_t * c)
{				       /*{{{ */
#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

#ifndef QTHREAD_NO_ASSERTS
    assert(t != NULL);
    assert(c != NULL);
#endif

    if (t->thread_state == QTHREAD_STATE_NEW) {

	qthread_debug("qthread_exec(%p, %p): type is QTHREAD_THREAD_NEW!\n",
		      t, c);
	t->thread_state = QTHREAD_STATE_RUNNING;

	getcontext(t->context);	       /* puts the current context into t->contextq */
	/* Several other libraries that do this reserve a few words on either
	 * end of the stack for some reason. To avoid problems, I'll also do
	 * this (even though I have no idea why they would do this). */
	/* t is cast here ONLY because the PGI compiler is idiotic about typedef's */
	t->context->uc_stack.ss_sp =
	    (char *)(((struct qthread_s *)t)->stack) + 8;
	t->context->uc_stack.ss_size = qlib->qthread_stack_size - 64;
#ifdef HAVE_CONTEXT_FUNCS
	/* the makecontext man page (Linux) says: set the uc_link FIRST
	 * why? no idea */
	t->context->uc_link = c;       /* NULL pthread_exit() */
	qthread_debug("qthread_exec(): context is {%p, %d, %p}\n",
		      t->context->uc_stack.ss_sp,
		      t->context->uc_stack.ss_size, t->context->uc_link);
#else
	qthread_debug("qthread_exec(): context is {%p, %d}\n",
		      t->context->uc_stack.ss_sp,
		      t->context->uc_stack.ss_size);
#endif
	makecontext(t->context, (void (*)(void))qthread_wrapper, 1, t);	/* the casting shuts gcc up */
#ifdef HAVE_CONTEXT_FUNCS
    } else {
	t->context->uc_link = c;       /* NULL pthread_exit() */
#endif
    }

    t->return_context = c;

#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_exec(%p): setting stack size limits... hopefully we don't currently exceed them!\n",
	 t);
    rlp.rlim_cur = qlib->qthread_stack_size;
    rlp.rlim_max = qlib->max_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif

    qthread_debug("qthread_exec(%p): executing swapcontext()...\n", t);
    /* return_context (aka "c") is being written over with the current context */
    if (swapcontext(t->return_context, t->context) != 0) {
	perror("qthread_exec: swapcontext() failed");
	abort();
    }
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_exec(%p): setting stack size limits back to normal...\n",
	 t);
    rlp.rlim_cur = qlib->master_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif

#ifndef QTHREAD_NO_ASSERTS
    assert(t != NULL);
    assert(c != NULL);
#endif

    qthread_debug("qthread_exec(%p): finished\n", t);
}				       /*}}} */

/* this function yields thread t to the master kernel thread */
void qthread_yield(qthread_t * t)
{				       /*{{{ */
    qthread_debug("qthread_yield(): thread %p yielding.\n", t);
    t->thread_state = QTHREAD_STATE_YIELDED;
    qthread_back_to_master(t);
    qthread_debug("qthread_yield(): thread %p resumed.\n", t);
}				       /*}}} */

/***********************************************
 * FORKING                                     *
 ***********************************************/
/* fork a thread by putting it in somebody's work queue
 * NOTE: scheduling happens here
 */
qthread_t *qthread_fork(const qthread_f f, const void *arg)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;

    /*
     * shep =
     * qthread_internal_atomic_inc_mod(&qlib->sched_kthread,
     * &qlib->sched_kthread_lock,
     * qlib->nkthreads); */
    ATOMIC_INC_MOD(shep, &qlib->sched_kthread, &qlib->sched_kthread_lock,
		   qlib->nkthreads);
    t = qthread_thread_new(f, arg, shep);


    qthread_debug("qthread_fork(): tid %u shep %u\n", t->thread_id, shep);

    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);
    qthread_enqueue(qlib->kthreads[shep].ready, t);

    return (t);
}				       /*}}} */

void qthread_fork_detach(const qthread_f f, const void *arg)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;

    /* shep =
     * qthread_internal_atomic_inc_mod(&qlib->sched_kthread,
     * &qlib->sched_kthread_lock,
     * qlib->nkthreads); */
    ATOMIC_INC_MOD(shep, &qlib->sched_kthread, &qlib->sched_kthread_lock,
		   qlib->nkthreads);

    t = qthread_thread_new(f, arg, shep);

    qthread_debug("qthread_fork_detach(): tid %u shep %u\n", t->thread_id,
		  shep);

    /* this is for qthread_join() */
    t->detached = 1;
    qthread_enqueue(qlib->kthreads[shep].ready, t);
}				       /*}}} */

qthread_t *qthread_fork_to(const qthread_f f, const void *arg,
			   const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nkthreads) {
	return NULL;
    }
    t = qthread_thread_new(f, arg, shepherd);
    qthread_debug("qthread_fork_to(): tid %u shep %u\n", t->thread_id,
		  shepherd);

    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);
    qthread_enqueue(qlib->kthreads[shepherd].ready, t);
    return (t);
}				       /*}}} */

void qthread_fork_to_detach(const qthread_f f, const void *arg,
			    const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nkthreads) {
	return;
    }
    t = qthread_thread_new(f, arg, shepherd);
    qthread_debug("qthread_fork_to_detach(): tid %u shep %u\n", t->thread_id,
		  shepherd);

    /* this is for qthread_join() */
    t->detached = 1;
    qthread_enqueue(qlib->kthreads[shepherd].ready, t);
}				       /*}}} */

void qthread_join(qthread_t * me, qthread_t * waitfor)
{				       /*{{{ */
    qthread_lock(me, waitfor);
    qthread_unlock(me, waitfor);
    qthread_thread_free((qthread_t *) waitfor);
    return;
}				       /*}}} */

static inline void qthread_back_to_master(qthread_t * t)
{				       /*{{{ */
#ifdef NEED_RLIMIT
    struct rlimit rlp;

    qthread_debug
	("qthread_back_to_master(%p): setting stack size limits for master thread...\n",
	 t);
    rlp.rlim_cur = qlib->master_stack_size;
    rlp.rlim_max = qlib->max_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif
    /* now back to your regularly scheduled master thread */
    if (swapcontext(t->context, t->return_context) != 0) {
	perror("qthread_back_to_master(): swapcontext() failed!");
	abort();
    }
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_back_to_master(%p): setting stack size limits back to qthread size...\n",
	 t);
    rlp.rlim_cur = qlib->qthread_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif
}				       /*}}} */

qthread_t *qthread_prepare(const qthread_f f, const void *arg)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;

    ATOMIC_INC_MOD(shep, &qlib->sched_kthread, &qlib->sched_kthread_lock,
		   qlib->nkthreads);

    t = qthread_thread_bare(f, arg, shep);
    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);

    return t;
}				       /*}}} */

qthread_t *qthread_prepare_detached(const qthread_f f, const void *arg)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;

    ATOMIC_INC_MOD(shep, &qlib->sched_kthread, &qlib->sched_kthread_lock,
		   qlib->nkthreads);

    t = qthread_thread_bare(f, arg, shep);
    t->detached = 1;

    return t;
}				       /*}}} */

qthread_t *qthread_prepare_for(const qthread_f f, const void *arg,
			       const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t = qthread_thread_bare(f, arg, shepherd);

    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);

    return t;
}				       /*}}} */

qthread_t *qthread_prepare_detached_for(const qthread_f f, const void *arg,
					const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t = qthread_thread_bare(f, arg, shepherd);

    t->detached = 1;

    return t;
}				       /*}}} */

void qthread_schedule(qthread_t * t)
{				       /*{{{ */
    qthread_thread_plush(t, t->shepherd);
    qthread_enqueue(qlib->kthreads[t->shepherd].ready, t);
}				       /*}}} */

void qthread_schedule_on(qthread_t * t, const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_thread_plush(t, shepherd);
    qthread_enqueue(qlib->kthreads[shepherd].ready, t);
}				       /*}}} */

/* functions to implement FEB locking/unlocking 
 *
 * NOTE: these have not been profiled, and so may need tweaking for speed
 * (e.g. multiple hash tables, shortening critical section, etc.)
 */

/* The lock ordering in these functions is very particular, and is designed to
 * reduce the impact of having only one hashtable. Don't monkey with it unless
 * you REALLY know what you're doing! If one hashtable becomes a problem, we
 * may need to move to a new mechanism.
 */

struct qthread_FEB_sub_args
{
    void *src;
    void *dest;
    pthread_mutex_t alldone;
};

/* this one is (strictly-speaking) unnecessary, but I think it helps with
 * optimization to have those consts */
struct qthread_FEB_ef_sub_args
{
    const size_t count;
    const void *dest;
    pthread_mutex_t alldone;
};

static inline qthread_addrstat_t *qthread_addrstat_new(const
						       qthread_shepherd_id_t
						       shepherd)
{				       /*{{{ */
    qthread_addrstat_t *ret = ALLOC_ADDRSTAT(shepherd);

    QTHREAD_INITLOCK(&ret->lock);
    ret->full = 1;
    ret->EFQ = NULL;
    ret->FEQ = NULL;
    ret->FFQ = NULL;
    return ret;
}				       /*}}} */

static inline void qthread_FEB_remove(void *maddr,
				      const qthread_shepherd_id_t threadshep)
{				       /*{{{ */
    qthread_addrstat_t *m;

    qthread_debug("qthread_FEB_remove(): attempting removal\n");
    cp_hashtable_wrlock(qlib->FEBs); {
	m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs, maddr);
	if (m) {
	    QTHREAD_LOCK(&(m->lock));
	    if (m->FEQ == NULL && m->EFQ == NULL && m->FFQ == NULL &&
		m->full == 1) {
		qthread_debug
		    ("qthread_FEB_remove(): all lists are empty, and status is full\n");
		cp_hashtable_remove(qlib->FEBs, maddr);
	    } else {
		m = NULL;
	    }
	}
    }
    cp_hashtable_unlock(qlib->FEBs);
    if (m != NULL) {
	QTHREAD_UNLOCK(&m->lock);
	QTHREAD_DESTROYLOCK(&m->lock);
	FREE_ADDRSTAT(threadshep, m);
    }
}				       /*}}} */

static inline void qthread_gotlock_empty(qthread_addrstat_t * m, void *maddr,
					 const qthread_shepherd_id_t
					 threadshep, const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;

    m->full = 0;
    if (m->EFQ != NULL) {
	/* dQ */
	X = m->EFQ;
	m->EFQ = X->next;
	/* op */
	memcpy(maddr, X->addr, WORDSIZE);
	m->full = 1;
	/* requeue */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	qthread_enqueue(qlib->kthreads[X->waiter->shepherd].ready, X->waiter);
	/* XXX: Note that this may not be the same mempool that this memory
	 * originally came from. This shouldn't be a big problem, but if it is,
	 * we will have to make the addrres_pool synchronized, and use the
	 * commented-out line. */
	/* FREE_ADDRRES(X->waiter->shepherd, X); */
	FREE_ADDRRES(threadshep, X);
	qthread_gotlock_fill(m, maddr, threadshep, 1);
    }
    if (m->full == 1 && m->EFQ == NULL && m->FEQ == NULL && m->FFQ == NULL)
	removeable = 1;
    else
	removeable = 0;
    if (!recursive) {
	QTHREAD_UNLOCK(&m->lock);
	if (removeable) {
	    qthread_FEB_remove(maddr, threadshep);
	}
    }
}				       /*}}} */

static inline void qthread_gotlock_fill(qthread_addrstat_t * m, void *maddr,
					const qthread_shepherd_id_t
					threadshep, const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;
    qthread_shepherd_id_t shepherd = 0;

    qthread_debug("qthread_gotlock_fill(%p, %p)\n", m, maddr);
    m->full = 1;
    /* dequeue all FFQ, do their operation, and schedule them */
    qthread_debug("qthread_gotlock_fill(): dQ all FFQ\n");
    while (m->FFQ != NULL) {
	/* dQ */
	X = m->FFQ;
	m->FFQ = X->next;
	/* op */
	memcpy(X->addr, maddr, WORDSIZE);
	/* schedule */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	shepherd = X->waiter->shepherd;
	qthread_enqueue(qlib->kthreads[shepherd].ready, X->waiter);
	/* XXX: Note that this may not be the same mempool that this memory
	 * originally came from. This shouldn't be a big problem, but if it is,
	 * we will have to make the addrres_pool synchronized, and use the
	 * commented-out line. */
	/* FREE_ADDRRES(X->waiter->shepherd, X); */
	FREE_ADDRRES(threadshep, X);
    }
    if (m->FEQ != NULL) {
	/* dequeue one FEQ, do their operation, and schedule them */
	qthread_t *waiter;

	qthread_debug("qthread_gotlock_fill(): dQ 1 FEQ\n");
	X = m->FEQ;
	m->FEQ = X->next;
	/* op */
	memcpy(X->addr, maddr, WORDSIZE);
	waiter = X->waiter;
	waiter->thread_state = QTHREAD_STATE_RUNNING;
	shepherd = waiter->shepherd;
	m->full = 0;
	qthread_enqueue(qlib->kthreads[shepherd].ready, waiter);
	/* XXX: Note that this may not be the same mempool that this memory
	 * originally came from. This shouldn't be a big problem, but if it is,
	 * we will have to make the addrres_pool synchronized, and use the
	 * commented-out line. */
	/* FREE_ADDRRES(shepherd, X); */
	FREE_ADDRRES(threadshep, X);
	qthread_gotlock_empty(m, maddr, threadshep, 1);
    }
    if (m->EFQ == NULL && m->FEQ == NULL && m->full == 1)
	removeable = 1;
    else
	removeable = 1;
    if (!recursive) {
	QTHREAD_UNLOCK(&m->lock);
	/* now, remove it if it needs to be removed */
	if (removeable) {
	    qthread_FEB_remove(maddr, threadshep);
	}
    }
}				       /*}}} */

static void qthread_empty_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_ef_sub_args *args =
	(struct qthread_FEB_ef_sub_args *)qthread_arg(me);

    qthread_empty(me, args->dest, args->count);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_empty(qthread_t * me, const void *dest, const size_t count)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	qthread_addrstat2_t *m_better;
	qthread_addrstat2_t *list = NULL;
	size_t i;
	aligned_t *startaddr;

	ALIGN(dest, startaddr, "qthread_empty()");
	cp_hashtable_wrlock(qlib->FEBs); {
	    for (i = 0; i < count; ++i) {
		m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							    (void *)(startaddr
								     + i));
		if (!m) {
		    m = qthread_addrstat_new(me->shepherd);
		    m->full = 0;
		    cp_hashtable_put(qlib->FEBs, (void *)(startaddr + i), m);
		} else {
		    QTHREAD_LOCK(&m->lock);
		    m_better = ALLOC_ADDRSTAT2(me->shepherd);
		    m_better->m = m;
		    m_better->addr = startaddr + i;
		    m_better->next = list;
		    list = m_better;
		}
	    }
	}
	cp_hashtable_unlock(qlib->FEBs);
	while (list != NULL) {
	    m_better = list;
	    list = list->next;
	    qthread_gotlock_empty(m_better->m, m_better->addr, me->shepherd,
				  0);
	    FREE_ADDRSTAT2(me->shepherd, m_better);
	}
    } else {
	struct qthread_FEB_ef_sub_args args =
	    { count, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_empty_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

static void qthread_fill_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_ef_sub_args *args =
	(struct qthread_FEB_ef_sub_args *)qthread_arg(me);

    qthread_fill(me, args->dest, args->count);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_fill(qthread_t * me, const void *dest, const size_t count)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat2_t *m_better;
	qthread_addrstat_t *m;
	qthread_addrstat2_t *list = NULL;
	size_t i;
	aligned_t *startaddr;

	ALIGN(dest, startaddr, "qthread_fill()");
	/* lock hash */
	cp_hashtable_wrlock(qlib->FEBs); {
	    for (i = 0; i < count; ++i) {
		m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							    (void *)(startaddr
								     + i));
		if (m) {
		    QTHREAD_LOCK(&m->lock);
		    m_better = ALLOC_ADDRSTAT2(me->shepherd);
		    m_better->m = m;
		    m_better->addr = startaddr + i;
		    m_better->next = list;
		    list = m_better;
		}
	    }
	}
	cp_hashtable_unlock(qlib->FEBs);	/* unlock hash */
	while (list != NULL) {
	    m_better = list;
	    list = list->next;
	    qthread_gotlock_fill(m_better->m, m_better->addr, me->shepherd,
				 0);
	    FREE_ADDRSTAT2(me->shepherd, m_better);
	}
    } else {
	struct qthread_FEB_ef_sub_args args =
	    { count, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_fill_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

/* the way this works is that:
 * 1 - data is copies from src to destination
 * 2 - the destination's FEB state gets changed from empty to full
 */

void qthread_writeF_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_sub_args *args =
	(struct qthread_FEB_sub_args *)qthread_arg(me);

    qthread_writeF(me, args->dest, args->src);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_writeF(qthread_t * me, void *dest, const void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	aligned_t *alignedaddr;

	ALIGN(dest, alignedaddr, "qthread_fill_with()");
	cp_hashtable_wrlock(qlib->FEBs); {	/* lock hash */
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd);
		cp_hashtable_put(qlib->FEBs, alignedaddr, m);
	    }
	    QTHREAD_LOCK(&m->lock);
	}
	cp_hashtable_unlock(qlib->FEBs);	/* unlock hash */
	/* we have the lock on m, so... */
	memcpy(dest, src, WORDSIZE);
	qthread_gotlock_fill(m, alignedaddr, me->shepherd, 0);
    } else {
	struct qthread_FEB_sub_args args =
	    { (void *)src, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_writeF_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

void qthread_writeF_const(qthread_t * me, void *dest, const aligned_t src)
{				       /*{{{ */
    qthread_writeF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state gets changed from empty to full
 */

static void qthread_writeEF_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_sub_args *args =
	(struct qthread_FEB_sub_args *)qthread_arg(me);

    qthread_writeEF(me, args->dest, args->src);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_writeEF(qthread_t * me, void *dest, const void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;

	qthread_debug("qthread_writeEF(%p, %p, %p): init\n", me, dest, src);
	ALIGN(dest, alignedaddr, "qthread_writeEF()");
	cp_hashtable_wrlock(qlib->FEBs); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd);
		cp_hashtable_put(qlib->FEBs, alignedaddr, m);
	    }
	    QTHREAD_LOCK(&(m->lock));
	}
	cp_hashtable_unlock(qlib->FEBs);
	qthread_debug("qthread_writeEF(): data structure locked\n");
	/* by this point m is locked */
	qthread_debug("qthread_writeEF(): m->full == %i\n", m->full);
	if (m->full == 1) {	       /* full, thus, we must block */
	    X = ALLOC_ADDRRES(me->shepherd);
	    X->addr = (aligned_t *) src;
	    X->waiter = me;
	    X->next = m->EFQ;
	    m->EFQ = X;
	} else {
	    memcpy(dest, src, WORDSIZE);
	    qthread_gotlock_fill(m, alignedaddr, me->shepherd, 0);
	}
	/* now all the addresses are either written or queued */
	qthread_debug("qthread_writeEF(): all written/queued\n");
	if (X) {
	    qthread_debug("qthread_writeEF(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
    } else {
	struct qthread_FEB_sub_args args =
	    { (void *)src, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_writeEF_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

void qthread_writeEF_const(qthread_t * me, void *dest, const aligned_t src)
{				       /*{{{ */
    qthread_writeEF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 */

static void qthread_readFF_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_sub_args *args =
	(struct qthread_FEB_sub_args *)qthread_arg(me);

    qthread_readFF(me, args->dest, args->src);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_readFF(qthread_t * me, void *dest, void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m = NULL;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;

	qthread_debug("qthread_readFF(%p, %p, %p): init\n", me, dest, src);
	ALIGN(src, alignedaddr, "qthread_readFF()");
	cp_hashtable_wrlock(qlib->FEBs); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)alignedaddr);
	    if (!m) {
		memcpy(dest, src, WORDSIZE);
	    } else {
		QTHREAD_LOCK(&m->lock);
	    }
	}
	cp_hashtable_unlock(qlib->FEBs);
	qthread_debug("qthread_readFF(): data structure locked\n");
	/* now m, if it exists, is locked - if m is NULL, then we're done! */
	if (m == NULL)
	    return;
	if (m->full != 1) {
	    X = ALLOC_ADDRRES(me->shepherd);
	    X->addr = (aligned_t *) dest;
	    X->waiter = me;
	    X->next = m->FFQ;
	    m->FFQ = X;
	} else {
	    memcpy(dest, src, WORDSIZE);
	    QTHREAD_UNLOCK(&m->lock);
	}
	/* if X exists, we are queued, and need to block (i.e. go back to the shepherd) */
	if (X) {
	    qthread_debug("qthread_readFF(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
    } else {
	struct qthread_FEB_sub_args args =
	    { src, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_readFF_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 * 3 - the src's FEB bits get changed from full to empty
 */

static void qthread_readFE_sub(qthread_t * me)
{				       /*{{{ */
    struct qthread_FEB_sub_args *args =
	(struct qthread_FEB_sub_args *)qthread_arg(me);

    qthread_readFE(me, args->dest, args->src);
    pthread_mutex_unlock(&args->alldone);
}				       /*}}} */

void qthread_readFE(qthread_t * me, void *dest, void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;

	qthread_debug("qthread_readFE(%p, %p, %p): init\n", me, dest, src);
	ALIGN(src, alignedaddr, "qthread_readFE()");
	cp_hashtable_wrlock(qlib->FEBs); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd);
		cp_hashtable_put(qlib->FEBs, alignedaddr, m);
	    }
	    QTHREAD_LOCK(&(m->lock));
	}
	cp_hashtable_unlock(qlib->FEBs);
	qthread_debug("qthread_readFE(): data structure locked\n");
	/* by this point m is locked */
	if (m->full == 0) {	       /* empty, thus, we must block */
	    X = ALLOC_ADDRRES(me->shepherd);
	    X->addr = (aligned_t *) dest;
	    X->waiter = me;
	    X->next = m->FEQ;
	    m->FEQ = X;
	} else {
	    memcpy(dest, src, WORDSIZE);
	    qthread_gotlock_empty(m, alignedaddr, me->shepherd, 0);
	}
	/* now all the addresses are either written or queued */
	if (X) {
	    qthread_debug("qthread_readFE(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
    } else {
	struct qthread_FEB_sub_args args =
	    { src, dest, PTHREAD_MUTEX_INITIALIZER };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_readFE_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
    }
}				       /*}}} */

/* functions to implement FEB-ish locking/unlocking
 *
 * These are atomic and functional, but do not have the same semantics as full
 * FEB locking/unlocking (for example, unlocking cannot block)
 *
 * NOTE: these have not been profiled, and so may need tweaking for speed
 * (e.g. multiple hash tables, shortening critical section, etc.)
 */

/* The lock ordering in these functions is very particular, and is designed to
 * reduce the impact of having only one hashtable. Don't monkey with it unless
 * you REALLY know what you're doing! If one hashtable becomes a problem, we
 * may need to move to a new mechanism.
 */

struct qthread_lock_sub_args
{
    pthread_mutex_t alldone;
    const void *addr;
};

static void qthread_lock_sub(qthread_t * t)
{				       /*{{{ */
    struct qthread_lock_sub_args *args =
	(struct qthread_lock_sub_args *)qthread_arg(t);

    qthread_lock(t, args->addr);
    pthread_mutex_unlock(&(args->alldone));
}				       /*}}} */

int qthread_lock(qthread_t * t, const void *a)
{				       /*{{{ */
    qthread_lock_t *m;

    if (t != NULL) {
	cp_hashtable_wrlock(qlib->locks);
	m = (qthread_lock_t *) cp_hashtable_get(qlib->locks, (void *)a);
	if (m == NULL) {
	    if ((m = ALLOC_LOCK(t->shepherd)) == NULL) {
		perror("qthread_lock()");
		abort();
	    }
	    m->waiting = qthread_queue_new(t->shepherd);
	    QTHREAD_INITLOCK(&m->lock);
	    cp_hashtable_put(qlib->locks, (void *)a, m);
	    /* since we just created it, we own it */
	    QTHREAD_LOCK(&m->lock);
	    /* can only unlock the hash after we've locked the address, because
	     * otherwise there's a race condition: the address could be removed
	     * before we have a chance to add ourselves to it */
	    cp_hashtable_unlock(qlib->locks);

	    m->owner = t->thread_id;
	    QTHREAD_UNLOCK(&m->lock);
	    qthread_debug("qthread_lock(%p, %p): returned (wasn't locked)\n",
			  t, a);
	} else {
	    /* success==failure: because it's in the hash, someone else owns the
	     * lock; dequeue this thread and yield.
	     * NOTE: it's up to the master thread to enqueue this thread and unlock
	     * the address
	     */
	    QTHREAD_LOCK(&m->lock);
	    /* for an explanation of the lock/unlock ordering here, see above */
	    cp_hashtable_unlock(qlib->locks);

	    t->thread_state = QTHREAD_STATE_BLOCKED;
	    t->blockedon = m;

	    qthread_back_to_master(t);

	    /* once I return to this context, I own the lock! */
	    /* conveniently, whoever unlocked me already set up everything too */
	    qthread_debug("qthread_lock(%p, %p): returned (was locked)\n", t,
			  a);
	}
	return 1;
    } else {
	struct qthread_lock_sub_args args = { PTHREAD_MUTEX_INITIALIZER, a };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_lock_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return 2;
    }
}				       /*}}} */

static void qthread_unlock_sub(qthread_t * t)
{				       /*{{{ */
    struct qthread_lock_sub_args *args =
	(struct qthread_lock_sub_args *)qthread_arg(t);

    qthread_unlock(t, args->addr);
    pthread_mutex_unlock(&(args->alldone));
}				       /*}}} */

int qthread_unlock(qthread_t * t, const void *a)
{				       /*{{{ */
    qthread_lock_t *m;
    qthread_t *u;

    qthread_debug("qthread_unlock(%p, %p): started\n", t, a);

    if (t != NULL) {
	cp_hashtable_wrlock(qlib->locks);
	m = (qthread_lock_t *) cp_hashtable_get(qlib->locks, (void *)a);

	if (m == NULL) {
#if 0
	    fprintf(stderr,
		    "qthread_unlock(%p,%p): attempt to unlock an address "
		    "that is not locked!\n", (void *)t, a);
	    abort();
#endif
	    cp_hashtable_unlock(qlib->locks);
	    return 1;
	}
	QTHREAD_LOCK(&m->lock);

	/* unlock the address... if anybody's waiting for it, give them the lock
	 * and put them in a ready queue.  If not, delete the lock structure.
	 */

	QTHREAD_LOCK(&m->waiting->lock);

	u = qthread_dequeue_nonblocking(m->waiting);
	if (u == NULL) {
	    qthread_debug("qthread_unlock(%p,%p): deleting waiting queue\n",
			  t, a);
	    cp_hashtable_remove(qlib->locks, (void *)a);
	    cp_hashtable_unlock(qlib->locks);

	    QTHREAD_UNLOCK(&m->waiting->lock);
	    /* XXX: Note that this may not be the same mempool that this memory
	     * originally came from. This shouldn't be a big problem, but if it is,
	     * we may have to get creative */
	    qthread_queue_free(m->waiting, t->shepherd);

	    QTHREAD_UNLOCK(&m->lock);
	    QTHREAD_DESTROYLOCK(&m->lock);
	    /* XXX: Note that this may not be the same mempool that this memory
	     * originally came from. This shouldn't be a big problem, but if it is,
	     * we may have to get creative */
	    FREE_LOCK(t->shepherd, m);
	} else {
	    cp_hashtable_unlock(qlib->locks);
	    qthread_debug
		("qthread_unlock(%p,%p): pulling thread from queue (%p)\n", t,
		 a, u);
	    u->thread_state = QTHREAD_STATE_RUNNING;
	    m->owner = u->thread_id;

	    /* NOTE: because of the use of getcontext()/setcontext(), threads
	     * return to the shepherd that setcontext()'d into them, so they
	     * must remain in that queue.
	     */
	    qthread_enqueue(qlib->kthreads[u->shepherd].ready, u);

	    QTHREAD_UNLOCK(&m->waiting->lock);
	    QTHREAD_UNLOCK(&m->lock);
	}

	qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
	return 1;
    } else {
	struct qthread_lock_sub_args args = { PTHREAD_MUTEX_INITIALIZER, a };

	QTHREAD_LOCK(&args.alldone);
	qthread_fork_detach(qthread_unlock_sub, &args);
	QTHREAD_LOCK(&args.alldone);
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);

	qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
	return 2;
    }
}				       /*}}} */

/* These are just accessor functions, in case we ever decide to make the qthread_t data structure opaque */
unsigned qthread_id(const qthread_t * t)
{				       /*{{{ */
    return t->shepherd;
}				       /*}}} */

void *qthread_arg(const qthread_t * t)
{				       /*{{{ */
    return t->arg;
}				       /*}}} */

qthread_shepherd_id_t qthread_shep(const qthread_t * t)
{				       /*{{{ */
    return t->shepherd;
}				       /*}}} */
