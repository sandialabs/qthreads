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

#ifdef POOLED
#define ALLOC_QTHREAD(shep)             (qthread_t *) cp_mempool_alloc(shep.qthread_pool)
#define ALLOC_QUEUE(shep)         (qthread_queue_t *) cp_mempool_alloc(qlib->kthreads[shep].queue_pool)
#define ALLOC_LOCK(shep)           (qthread_lock_t *) cp_mempool_alloc(qlib->kthreads[shep].lock_pool)
#define ALLOC_ADDRRES(shep)     (qthread_addrres_t *) cp_mempool_alloc(qlib->kthreads[shep].addrres_pool)
#define ALLOC_ADDRSTAT(shep)   (qthread_addrstat_t *) cp_mempool_alloc(qlib->kthreads[shep].addrstat_pool)
#define ALLOC_ADDRSTAT2(shep) (qthread_addrstat2_t *) cp_mempool_alloc(qlib->kthreads[shep].addrstat2_pool)
#define ALLOC_STACK(shep)                             cp_mempool_alloc(qlib->kthreads[shep].stack_pool)
#define ALLOC_CONTEXT(shep)            (ucontext_t *) cp_mempool_alloc(qlib->kthreads[shep].context_pool)
#define FREE(pool, t) cp_mempool_free(pool, t)
#else
#define ALLOC_QTHREAD(shep)             (qthread_t *) malloc(sizeof(qthread_t))
#define ALLOC_QUEUE(shep)         (qthread_queue_t *) malloc(sizeof(qthread_queue_t))
#define ALLOC_LOCK(shep)           (qthread_lock_t *) malloc(sizeof(qthread_lock_t))
#define ALLOC_ADDRRES(shep)     (qthread_addrres_t *) malloc(sizeof(qthread_addrres_t))
#define ALLOC_ADDRSTAT(shep)   (qthread_addrstat_t *) malloc(sizeof(qthread_addrstat_t))
#define ALLOC_ADDRSTAT2(shep) (qthread_addrstat2_t *) malloc(sizeof(qthread_addrstat2_t))
#define ALLOC_STACK(shep)                             malloc(stack_size)
#define ALLOC_CONTEXT(shep)            (ucontext_t *) malloc(sizeof(ucontext_t))
#define FREE(pool, t) free(t)
#endif

/* internal data structures */
typedef struct qthread_lock_s qthread_lock_t;
typedef struct qthread_shepherd_s qthread_shepherd_t;
typedef struct qthread_queue_s qthread_queue_t;
typedef unsigned char shepherd_id_t;	/* doubt we'll run more than 255 shepherds */

struct qthread_s
{
    unsigned int thread_id;
    unsigned char thread_state;
    unsigned char detached:1;

    /* the pthread we run on */
    shepherd_id_t shepherd;
    /* a pointer used for passing information back to the shepherd when
     * becoming blocked */
    struct qthread_lock_s *blockedon;

    /* the function to call (that defines this thread) */
    void (*f) (struct qthread_s *);
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

typedef struct qthread_addrstat_s
{
    pthread_mutex_t lock;
    cp_list *EFQ;
    cp_list *FEQ;
    cp_list *FFQ;
    unsigned int full:1;
} qthread_addrstat_t;

typedef struct qthread_addrstat2_s
{
    qthread_addrstat_t *m;
    char *addr;
} qthread_addrstat2_t;

typedef struct qthread_addrres_s
{
    char *data;			/* ptr to the memory NOT being blocked on */
    char *beginning;		/* ptr to the memory being blocked on */
    size_t ctr;
    pthread_mutex_t ctr_lock;
    qthread_t *waiter;
} qthread_addrres_t;

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
static void *qthread_shepherd(void *arg);
static void qthread_wrapper(void *arg);

static void qthread_FEBlock_delete(qthread_addrstat_t * m);
static inline qthread_t *qthread_thread_new(qthread_f f, void *arg,
					    shepherd_id_t shepherd);
static inline void qthread_thread_free(qthread_t * t);
static inline void qthread_stack_new(qthread_t * t, unsigned stack_size);
static inline qthread_queue_t *qthread_queue_new(shepherd_id_t shepherd);
static inline void qthread_queue_free(qthread_queue_t * q,
				      shepherd_id_t shepherd);
static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t);
static inline qthread_t *qthread_dequeue(qthread_queue_t * q);
static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q);
static inline void qthread_exec(qthread_t * t, ucontext_t * c);
static inline void qthread_back_to_master(qthread_t * t);

static inline unsigned qthread_internal_atomic_inc(unsigned *x,
						   pthread_mutex_t * lock,
						   int inc)
{				       /*{{{ */
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    *x = *x + inc;
    pthread_mutex_unlock(lock);
    return (r);
}				       /*}}} */

static inline unsigned qthread_internal_atomic_inc_mod(unsigned *x,
						       pthread_mutex_t * lock,
						       int inc, int mod)
{				       /*{{{ */
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    *x = (*x + inc) % mod;
    pthread_mutex_unlock(lock);
    return (r);
}				       /*}}} */

#if 0				       /* currently not used */
static inline unsigned qthread_internal_atomic_check(unsigned *x,
						     pthread_mutex_t * lock)
{				       /*{{{ */
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    pthread_mutex_unlock(lock);

    return (r);
}				       /*}}} */
#endif

static void qthread_internal_unlock_locks(void *a)
{				       /*{{{ */
    assert(pthread_mutex_unlock((pthread_mutex_t *) a) == 0);
}				       /*}}} */

/* the qthread_shepherd() is the pthread responsible for actually
 * executing the work units
 */

/*#define QTHREAD_DEBUG 1*/
/* for debugging */
#ifdef QTHREAD_DEBUG
static inline void qthread_debug(char *format, ...)
{				       /*{{{ */
    static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;
    va_list args;

    pthread_mutex_lock(&output_lock);

    fprintf(stderr, "qthread_debug(): ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);		       /* KBW: helps keep things straight */

    pthread_mutex_unlock(&output_lock);
}				       /*}}} */
#else
#define qthread_debug(...) do{ }while(0)
#endif

/* this function is the workhorse of the library: this is the function that
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
	    assert((t->thread_state == QTHREAD_STATE_NEW) ||
		   (t->thread_state == QTHREAD_STATE_RUNNING));

	    assert(t->f != NULL);

	    /* note: there's a good argument that the following should
	     * be: (*t->f)(t), however the state management would be
	     * more complex 
	     */

	    assert(t->shepherd == me->kthread_index);
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
			("qthread_shepherd(%u): unlocking FEB address locks\n",
			 me->kthread_index);
		    t->thread_state = QTHREAD_STATE_BLOCKED;
		    {
			qthread_addrres_t *X =
			    (qthread_addrres_t *) (t->blockedon);
			cp_list *locks = (cp_list *) (X->waiter);

			X->waiter = t;
			t->blockedon = NULL;
			cp_list_destroy_custom(locks,
					       qthread_internal_unlock_locks);
		    }
		    break;

		case QTHREAD_STATE_BLOCKED:	/* put it in the blocked queue */
		    qthread_debug
			("qthread_shepherd(%u): adding blocked thread %p to blocked queue\n",
			 me->kthread_index, t);
		    qthread_enqueue((qthread_queue_t *) t->blockedon->waiting,
				    t);
		    assert(pthread_mutex_unlock(&(t->blockedon->lock)) == 0);
		    break;

		case QTHREAD_STATE_TERMINATED:
		    qthread_debug
			("qthread_shepherd(%u): thread %p is in state terminated.\n",
			 me->kthread_index, t);
		    t->thread_state = QTHREAD_STATE_DONE;
		    if (t->detached == 0) {
			/* rely on the joiner to clean up this memory */
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

int qthread_init(int nkthreads)
{				       /*{{{ */
    int i, r;

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
    assert(pthread_mutex_init(&qlib->sched_kthread_lock, NULL) == 0);
    assert(pthread_mutex_init(&qlib->max_thread_id_lock, NULL) == 0);

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
		cp_mempool_create_by_option(0, 4096, 1000);
	} else {
	    qlib->kthreads[i].stack_pool =
		cp_mempool_create_by_option(0, qlib->qthread_stack_size,
					    1000);
	}
	/* this prevents an alignment problem. */
	if (sizeof(ucontext_t) < 2048) {
	    qlib->kthreads[i].context_pool =
		cp_mempool_create_by_option(0, 2048, 1000);
	} else {
	    qlib->kthreads[i].context_pool =
		cp_mempool_create_by_option(0, sizeof(ucontext_t), 1000);
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
	qlib->kthreads[i].ready = qthread_queue_new((shepherd_id_t) - 1);

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

    assert(qlib != NULL);

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
	qthread_queue_free(qlib->kthreads[i].ready, (shepherd_id_t) - 1);
    }

    cp_hashtable_destroy(qlib->locks);
    cp_hashtable_destroy_custom(qlib->FEBs, NULL, (cp_destructor_fn)
				qthread_FEBlock_delete);
    cp_hashtable_destroy(p_to_shep);

    assert(pthread_mutex_destroy(&qlib->sched_kthread_lock) == 0);
    assert(pthread_mutex_destroy(&qlib->max_thread_id_lock) == 0);

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
    t = qlib->kthreads[((shepherd_id_t) (size_t) ret) - 1].current;
    /* printf("stack pointer should be %p\n", t->stack); */
    return t;
}				       /*}}} */

/************************************************************/
/* functions to manage thread stack allocation/deallocation */
/************************************************************/

static inline qthread_t *qthread_thread_new(qthread_f f, void *arg,
					    shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;
    ucontext_t *uc;

    t = ALLOC_QTHREAD((qlib->kthreads[shepherd]));
    if (t == NULL) {
	perror("qthread_thread_new()");
	abort();
    }

    uc = ALLOC_CONTEXT(shepherd);
    if (uc == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->context = uc;

    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = arg;
    t->stack = NULL;
    t->blockedon = NULL;
    t->shepherd = shepherd;

    return (t);
}				       /*}}} */

static inline void qthread_thread_free(qthread_t * t)
{				       /*{{{ */
    qthread_shepherd_t *shep;

    assert(t != NULL);

    shep = &(qlib->kthreads[t->shepherd]);
    FREE(shep->context_pool, t->context);
    if (t->stack != NULL) {
	FREE(shep->stack_pool, t->stack);
    }
    FREE(shep->qthread_pool, t);
}				       /*}}} */


static inline void qthread_stack_new(qthread_t * t, unsigned stack_size)
{				       /*{{{ */
    void *s;

    s = ALLOC_STACK(t->shepherd);
    if (s == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
    t->stack = s;
}				       /*}}} */

/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

static inline qthread_queue_t *qthread_queue_new(shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_queue_t *q;

    if (shepherd == (shepherd_id_t) - 1) {
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
    assert(pthread_mutex_init(&q->lock, NULL) == 0);
    assert(pthread_cond_init(&q->notempty, NULL) == 0);
    return (q);
}				       /*}}} */

static inline void qthread_queue_free(qthread_queue_t * q,
				      shepherd_id_t shepherd)
{				       /*{{{ */
    assert((q->head == NULL) && (q->tail == NULL));
    assert(pthread_mutex_destroy(&q->lock) == 0);
    assert(pthread_cond_destroy(&q->notempty) == 0);
    if (shepherd == (shepherd_id_t) - 1) {
	free(q);
    } else {
	FREE(qlib->kthreads[shepherd].queue_pool, q);
    }
}				       /*}}} */

static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t)
{				       /*{{{ */
    assert(t != NULL);
    assert(q != NULL);

    qthread_debug("qthread_enqueue(%p,%p): started\n", q, t);

    assert(pthread_mutex_lock(&q->lock) == 0);

    t->next = NULL;

    if ((q->head == NULL) && (q->tail == NULL)) {
	q->head = t;
	q->tail = t;
	assert(pthread_cond_signal(&q->notempty) == 0);
    } else {
	q->tail->next = t;
	q->tail = t;
    }

    qthread_debug("qthread_enqueue(%p,%p): finished\n", q, t);
    assert(pthread_mutex_unlock(&q->lock) == 0);
}				       /*}}} */

static inline qthread_t *qthread_dequeue(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t = NULL;

    qthread_debug("qthread_dequeue(%p,%p): started\n", q, t);

    assert(pthread_mutex_lock(&q->lock) == 0);

    while ((q->head == NULL) && (q->tail == NULL)) {
	assert(pthread_cond_wait(&q->notempty, &q->lock) == 0);
    }

    assert(q->head != NULL);

    if (q->head != q->tail) {
	t = q->head;
	q->head = q->head->next;
	t->next = NULL;
    } else {
	t = q->head;
	q->head = q->head->next;
	t->next = NULL;
	q->tail = NULL;
    }

    assert(pthread_mutex_unlock(&q->lock) == 0);

    qthread_debug("qthread_dequeue(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */

static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t = NULL;

    /* NOTE: it's up to the caller to lock/unlock the queue! */
    qthread_debug("qthread_dequeue_nonblocking(%p,%p): started\n", q, t);

    if ((q->head == NULL) && (q->tail == NULL)) {
	qthread_debug
	    ("qthread_dequeue_nonblocking(%p,%p): finished (nobody in list)\n",
	     q, t);
	return (NULL);
    }

    if (q->head != q->tail) {
	t = q->head;
	q->head = q->head->next;
	t->next = NULL;
    } else {
	t = q->head;
	q->head = q->head->next;
	t->next = NULL;
	q->tail = NULL;
    }

    qthread_debug("qthread_dequeue_nonblocking(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */

/* this function is for maintenance of the FEB hashtables. SHOULD only be
 * necessary for things left over when qthread_finalize is called */
static void qthread_FEBlock_delete(qthread_addrstat_t * m)
{				       /*{{{ */
    cp_list_destroy(m->EFQ);
    cp_list_destroy(m->FEQ);
    cp_list_destroy(m->FFQ);
    /* NOTE! This is only safe if this function (as part of destroying the FEB
     * hash table) is ONLY called once all other pthreads have been joined */
    FREE(qlib->kthreads[0].addrstat_pool, m);
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
#ifndef HAVE_CONTEXT_FUNCS
    /* without a built-in make/get/swapcontext, we're relying on the portable
     * one in context.c (stolen from libtask). unfortunately, this home-made
     * context stuff does not allow us to set up a uc_link pointer that will be
     * returned to once qthread_wrapper returns, so we have to do it by hand:
     */
    qthread_back_to_master(t);
#endif
}				       /*}}} */

static inline void qthread_exec(qthread_t * t, ucontext_t * c)
{				       /*{{{ */
#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    assert(t != NULL);
    assert(c != NULL);

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

    assert(t != NULL);
    assert(c != NULL);

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

static inline qthread_t *qthread_fork_internal(qthread_f f, void *arg,
					       shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    t = qthread_thread_new(f, arg, shepherd);	/* new thread struct sans stack */
    qthread_stack_new(t, qlib->qthread_stack_size);	/* fill in stack */

    qthread_debug
	("qthread_fork_internal(): creating qthread %p with stack %p\n", t,
	 t->stack);

    /* figure out which queue to put the thread into */
    t->thread_id =
	qthread_internal_atomic_inc(&qlib->max_thread_id,
				    &qlib->max_thread_id_lock, 1);

    return t;
}				       /*}}} */

/* fork a thread by putting it in somebody's work queue
 * NOTE: scheduling happens here
 */
qthread_t *qthread_fork(qthread_f f, void *arg)
{				       /*{{{ */
    qthread_t *t;
    unsigned int shep;

    shep =
	qthread_internal_atomic_inc_mod(&qlib->sched_kthread,
					&qlib->sched_kthread_lock, 1,
					qlib->nkthreads);
    t = qthread_fork_internal(f, arg, shep);


    qthread_debug("qthread_fork(): tid %u shep %u\n", t->thread_id, shep);

    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);
    qthread_enqueue(qlib->kthreads[shep].ready, t);

    return (t);
}				       /*}}} */

void qthread_fork_detach(qthread_f f, void *arg)
{				       /*{{{ */
    qthread_t *t;
    unsigned int shep;

    shep =
	qthread_internal_atomic_inc_mod(&qlib->sched_kthread,
					&qlib->sched_kthread_lock, 1,
					qlib->nkthreads);

    t = qthread_fork_internal(f, arg, shep);

    qthread_debug("qthread_fork_detach(): tid %u shep %u\n", t->thread_id,
		  shep);

    /* this is for qthread_join() */
    t->detached = 1;
    qthread_enqueue(qlib->kthreads[shep].ready, t);
}				       /*}}} */

qthread_t *qthread_fork_to(qthread_f f, void *arg, unsigned shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nkthreads) {
	return NULL;
    }
    t = qthread_fork_internal(f, arg, shepherd);
    qthread_debug("qthread_fork_to(): tid %u shep %u\n", t->thread_id,
		  shepherd);

    /* this is for qthread_join() */
    t->detached = 0;
    qthread_lock(t, t);
    qthread_enqueue(qlib->kthreads[shepherd].ready, t);
    return (t);
}				       /*}}} */

void qthread_fork_to_detach(qthread_f f, void *arg, unsigned shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nkthreads) {
	return;
    }
    t = qthread_fork_internal(f, arg, shepherd);
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

void qthread_busy_join(volatile qthread_t * waitfor)
{				       /*{{{ */
    /* this is extremely inefficient! */
    while (waitfor->thread_state != QTHREAD_STATE_DONE) ;
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

static inline qthread_addrstat_t *qthread_addrstat_new(shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_addrstat_t *ret = ALLOC_ADDRSTAT(shepherd);
    cp_mempool *lp = qlib->kthreads[shepherd].list_pool;

    assert(pthread_mutex_init(&ret->lock, NULL) == 0);
    ret->full = 1;
    ret->EFQ = cp_list_create_nosync();
    ret->FEQ = cp_list_create_nosync();
    ret->FFQ = cp_list_create_nosync();
#ifdef POOLED
    cp_list_use_mempool(ret->EFQ, lp);
    cp_list_use_mempool(ret->FEQ, lp);
    cp_list_use_mempool(ret->FFQ, lp);
#endif
    return ret;
}				       /*}}} */

static inline void qthread_gotlock_empty(qthread_addrstat_t * m, char *maddr)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;

    m->full = 0;
    if (!cp_list_is_empty(m->EFQ)) {
	X = (qthread_addrres_t *) cp_list_remove_head(m->EFQ);
	qthread_debug("gotlock_emtpy: X->data:%p X->beginning:%p maddr:%p\n",
		      X->data, X->beginning, maddr);
	*maddr = X->data[maddr - X->beginning];
	assert(pthread_mutex_lock(&X->ctr_lock) == 0);
	X->ctr--;
	if (X->ctr == 0) {
	    X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	    qthread_enqueue(qlib->kthreads[X->waiter->shepherd].ready,
			    X->waiter);
	    assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	    assert(pthread_mutex_destroy(&X->ctr_lock) == 0);
	    FREE(qlib->kthreads[X->waiter->shepherd].addrres_pool, X);
	} else {
	    assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	}
	m->full = 1;
    }
    assert(pthread_mutex_unlock(&m->lock) == 0);
}				       /*}}} */

static inline void qthread_gotlock_fill(qthread_addrstat_t * m,
					const char *maddr)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable = 1;
    shepherd_id_t shepherd = 0;

    qthread_debug("qthread_gotlock_fill(%p, %p)\n", m, maddr);
    m->full = 1;
    if (cp_list_item_count(m->FFQ) > 0) {
	/* dequeue all FFQ, do their operation, and schedule them */
	qthread_debug("qthread_gotlock_fill(): dQ all FFQ\n");
	while (!cp_list_is_empty(m->FFQ)) {
	    /* dQ */
	    X = (qthread_addrres_t *) cp_list_remove_head(m->FFQ);
	    /* op */
	    X->data[maddr - X->beginning] = *maddr;
	    assert(pthread_mutex_lock(&X->ctr_lock) == 0);
	    X->ctr--;
	    /* schedule */
	    if (X->ctr == 0) {
		X->waiter->thread_state = QTHREAD_STATE_RUNNING;
		shepherd = X->waiter->shepherd;
		qthread_enqueue(qlib->kthreads[shepherd].ready, X->waiter);
		assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
		assert(pthread_mutex_destroy(&X->ctr_lock) == 0);
		FREE(qlib->kthreads[shepherd].addrres_pool, X);
	    } else {
		assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	    }
	}
    }
    if (cp_list_item_count(m->FEQ) > 0) {
	/* dequeue one FEQ, do their operation, and schedule them */
	qthread_debug("qthread_gotlock_fill(): dQ 1 FEQ\n");
	X = (qthread_addrres_t *) cp_list_remove_head(m->FEQ);
	/* op */
	X->data[maddr - X->beginning] = *maddr;
	assert(pthread_mutex_lock(&X->ctr_lock) == 0);
	X->ctr--;
	if (X->ctr == 0) {
	    X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	    shepherd = X->waiter->shepherd;
	    qthread_enqueue(qlib->kthreads[shepherd].ready, X->waiter);
	    assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	    assert(pthread_mutex_destroy(&X->ctr_lock) == 0);
	    FREE(qlib->kthreads[shepherd].addrres_pool, X);
	} else {
	    assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	}
	m->full = 0;
	removeable = 0;
    }
    if (removeable && cp_list_item_count(m->EFQ) != 0) {
	removeable = 0;
    }
    assert(pthread_mutex_unlock(&m->lock) == 0);
    /* now, remove it if it needs to be removed */
    if (removeable) {
	int removed = 0;

	qthread_debug("qthread_gotlock_fill(): removeable\n");
	cp_hashtable_wrlock(qlib->FEBs); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)maddr);
	    if (m) {
		assert(pthread_mutex_lock(&(m->lock)) == 0);
		if (cp_list_is_empty(m->FEQ) && cp_list_is_empty(m->EFQ) &&
		    cp_list_is_empty(m->FFQ) && m->full == 1) {
		    qthread_debug
			("qthread_gotlock_fill(): all lists are empty, and status is full\n");
		    cp_hashtable_remove(qlib->FEBs, (void *)maddr);
		    removed = 1;
		}
		assert(pthread_mutex_unlock(&m->lock) == 0);
	    }
	} cp_hashtable_unlock(qlib->FEBs);
	if (removed == 1) {
	    cp_list_destroy(m->FEQ);
	    cp_list_destroy(m->EFQ);
	    cp_list_destroy(m->FFQ);
	    assert(pthread_mutex_destroy(&m->lock) == 0);
	    FREE(qlib->kthreads[shepherd].addrstat_pool, m);
	}
    }
}				       /*}}} */

void qthread_empty(qthread_t * t, char *dest, const size_t bytes)
{				       /*{{{ */
    cp_list *list;
    cp_list_iterator it;
    qthread_addrstat_t *m;
    qthread_addrstat2_t *m_better;
    size_t i;

    list = cp_list_create_nosync();
#ifdef POOLED
    cp_list_use_mempool(list, qlib->kthreads[t->shepherd].list_pool);
#endif
    cp_hashtable_wrlock(qlib->FEBs); {
	for (i = 0; i < bytes; ++i) {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)(dest + i));
	    if (!m) {
		m = qthread_addrstat_new(t->shepherd);
		m->full = 0;
		cp_hashtable_put(qlib->FEBs, (void *)(dest + i), m);
	    } else {
		assert(pthread_mutex_lock(&m->lock) == 0);
		m_better = ALLOC_ADDRSTAT2(t->shepherd);
		m_better->m = m;
		m_better->addr = dest + i;
		cp_list_append(list, m_better);
	    }
	}
    }
    cp_hashtable_unlock(qlib->FEBs);
    cp_list_iterator_init(&it, list, COLLECTION_LOCK_NONE);
    while ((m_better =
	    (qthread_addrstat2_t *) cp_list_iterator_next(&it)) != NULL) {
	qthread_gotlock_empty(m_better->m, m_better->addr);
	FREE(qlib->kthreads[t->shepherd].addrstat2_pool, m_better);
    }
    cp_list_iterator_release(&it);
}				       /*}}} */

void qthread_fill(qthread_t * t, char *dest, const size_t bytes)
{				       /*{{{ */
    qthread_addrstat2_t *m_better;
    qthread_addrstat_t *m;
    cp_list *list;
    cp_list_iterator it;
    size_t i;

    list = cp_list_create_nosync();
#ifdef POOLED
    cp_list_use_mempool(list, qlib->kthreads[t->shepherd].list_pool);
#endif
    cp_hashtable_wrlock(qlib->FEBs); {
	for (i = 0; i < bytes; ++i) {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)(dest + i));
	    if (m) {
		assert(pthread_mutex_lock(&m->lock) == 0);
		m_better = ALLOC_ADDRSTAT2(t->shepherd);
		m_better->m = m;
		m_better->addr = (char *)(dest + i);
		cp_list_append(list, m_better);
	    }
	}
    } cp_hashtable_unlock(qlib->FEBs);
    cp_list_iterator_init(&it, list, COLLECTION_LOCK_NONE);
    while ((m_better =
	    (qthread_addrstat2_t *) cp_list_iterator_next(&it)) != NULL) {
	qthread_gotlock_fill(m_better->m, m_better->addr);
	FREE(qlib->kthreads[t->shepherd].addrstat2_pool, m_better);
    }
    cp_list_iterator_release(&it);
}				       /*}}} */

/* the way this works is that:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state get changed from empty to full
 */
void qthread_writeEF_size(qthread_t * t, char *dest, char *src,
			  const size_t bytes)
{				       /*{{{ */
    size_t i;
    qthread_addrstat_t **m;
    qthread_addrres_t *X = NULL;
    cp_list *queuedlocks = NULL;

    qthread_debug("qthread_writeEF_size(%p, %p, %p, %u): init\n", t, dest,
		  src, (unsigned)bytes);
    m = (qthread_addrstat_t **) malloc(sizeof(qthread_addrstat_t *) * bytes);
    cp_hashtable_wrlock(qlib->FEBs); {
	for (i = 0; i < bytes; ++i) {
	    m[i] =
		(qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs, dest + i);
	    if (!m[i]) {
		m[i] = qthread_addrstat_new(t->shepherd);
		cp_hashtable_put(qlib->FEBs, dest + i, m[i]);
	    }
	    assert(pthread_mutex_lock(&(m[i]->lock)) == 0);
	}
    }
    cp_hashtable_unlock(qlib->FEBs);
    qthread_debug("qthread_writeEF_size(): data structures locked\n");
    /* by this point, all the address data structures (m's) are locked */
    for (i = 0; i < bytes; i++) {
	qthread_debug("qthread_writeEF_size(): m[%i]->full == %i\n", i,
		      m[i]->full);
	if (m[i]->full) {
	    if (!X) {
		X = ALLOC_ADDRRES(t->shepherd);
		pthread_mutex_init(&X->ctr_lock, NULL);
		X->waiter = t;
		X->data = src;
		X->beginning = dest;
		X->ctr = 0;
		queuedlocks = cp_list_create_nosync();
#ifdef POOLED
		cp_list_use_mempool(queuedlocks,
				    qlib->kthreads[t->shepherd].list_pool);
#endif
		qthread_debug
		    ("qthread_writeEF_size(): X->data:%p X->beginning:%p\n",
		     X->data, X->beginning);
	    }
	    cp_list_append(m[i]->EFQ, X);
	    X->ctr++;
	    cp_list_append(queuedlocks, &(m[i]->lock));
	} else {
	    dest[i] = src[i];
	    qthread_gotlock_fill(m[i], dest + i);
	}
    }
    /* now all the addresses are either written or queued */
    qthread_debug("qthread_writeEF_size(): all written/queued\n");
    free(m);			       /* not pooled memory */
    if (X) {
	qthread_debug("qthread_writeEF_size(): back to parent\n");
	t->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	/* these are a bit screwey, to save on memory */
	t->blockedon = (struct qthread_lock_s *)X;
	X->waiter = (qthread_t *) queuedlocks;
	qthread_back_to_master(t);
    }
}				       /*}}} */

void qthread_writeEF(qthread_t * t, int *dest, int src)
{				       /*{{{ */
    qthread_writeEF_size(t, (char *)dest, (char *)&src, sizeof(int));
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 */
void qthread_readFF_size(qthread_t * t, char *dest, char *src,
			 const size_t bytes)
{				       /*{{{ */
    size_t i;
    qthread_addrstat2_t *m_better;
    qthread_addrstat_t *m;
    qthread_addrres_t *X = NULL;
    cp_list *list, *queuedlocks = NULL;
    cp_list_iterator it;

    list = cp_list_create_nosync();
#ifdef POOLED
    cp_list_use_mempool(list, qlib->kthreads[t->shepherd].list_pool);
#endif
    cp_hashtable_wrlock(qlib->FEBs); {
	for (i = 0; i < bytes; ++i) {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs,
							(void *)(src + i));
	    if (!m) {
		dest[i] = src[i];
	    } else {
		m_better = ALLOC_ADDRSTAT2(t->shepherd);
		m_better->m = m;
		m_better->addr = (char *)(src + i);
		cp_list_append(list, m_better);
		assert(pthread_mutex_lock(&m->lock) == 0);
	    }
	    assert(pthread_mutex_lock(&m->lock) == 0);
	}
    } cp_hashtable_unlock(qlib->FEBs);
    /* by this point, all the unread address data structures (m's) are locked */
    cp_list_iterator_init(&it, list, COLLECTION_LOCK_NONE);
    while ((m_better =
	    (qthread_addrstat2_t *) cp_list_iterator_remove(&it)) != NULL) {
	if (!m_better->m->full) {
	    if (!X) {
		X = ALLOC_ADDRRES(t->shepherd);
		assert(pthread_mutex_init(&X->ctr_lock, NULL) == 0);
		X->waiter = t;
		X->ctr = 0;
		X->data = dest;
		X->beginning = src;
		queuedlocks = cp_list_create_nosync();
#ifdef POOLED
		cp_list_use_mempool(queuedlocks,
				    qlib->kthreads[t->shepherd].list_pool);
#endif
	    }
	    cp_list_append(m_better->m->FFQ, X);
	    assert(pthread_mutex_lock(&X->ctr_lock) == 0);
	    X->ctr++;
	    assert(pthread_mutex_unlock(&X->ctr_lock) == 0);
	    cp_list_append(queuedlocks, &(m_better->m->lock));
	} else {
	    int offset = m_better->addr - src;

	    dest[offset] = src[offset];
	    assert(pthread_mutex_unlock(&m->lock) == 0);
	}
	FREE(qlib->kthreads[t->shepherd].addrstat2_pool, m_better);
    }
    cp_list_iterator_release(&it);
    cp_list_destroy(list);
    /* by this point, all that remains in the list are addresses that are empty & queued */
    if (X) {
	/* something was queued */
	t->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	/* these are a bit screwey, to save on memory */
	t->blockedon = (struct qthread_lock_s *)X;
	X->waiter = (qthread_t *) queuedlocks;
	qthread_back_to_master(t);
    }
}				       /*}}} */

void qthread_readFF(qthread_t * t, int *dest, int *src)
{				       /*{{{ */
    qthread_readFF_size(t, (char *)dest, (char *)&src, sizeof(int));
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 * 3 - the src's FEB bits get changed from full to empty
 */
void qthread_readFE_size(qthread_t * t, char *dest, char *src,
			 const size_t bytes)
{				       /*{{{ */
    size_t i;
    qthread_addrstat_t **m;
    qthread_addrres_t *X = NULL;
    cp_list *queuedlocks = NULL;

    qthread_debug("qthread_readFE_size(%p, %p, %p, %u): init\n", t, dest, src,
		  (unsigned)bytes);
    m = (qthread_addrstat_t **) malloc(sizeof(qthread_addrstat_t *) * bytes);
    cp_hashtable_wrlock(qlib->FEBs); {
	for (i = 0; i < bytes; ++i) {
	    m[i] =
		(qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs, src + i);
	    if (!m[i]) {
		m[i] = qthread_addrstat_new(t->shepherd);
		cp_hashtable_put(qlib->FEBs, src + i, m[i]);
	    }
	    assert(pthread_mutex_lock(&(m[i]->lock)) == 0);
	}
    }
    cp_hashtable_unlock(qlib->FEBs);
    /* by this point, all the address data structures (m's) are locked */
    for (i = 0; i < bytes; ++i) {
	if (!m[i]->full) {
	    if (!X) {
		X = ALLOC_ADDRRES(t->shepherd);
		pthread_mutex_init(&X->ctr_lock, NULL);
		X->waiter = t;
		X->data = dest;
		X->beginning = src;
		X->ctr = 0;
		queuedlocks = cp_list_create_nosync();
#ifdef POOLED
		cp_list_use_mempool(queuedlocks,
				    qlib->kthreads[t->shepherd].list_pool);
#endif
	    }
	    cp_list_append(m[i]->FEQ, X);
	    X->ctr++;
	    cp_list_append(queuedlocks, &(m[i]->lock));
	} else {
	    dest[i] = src[i];
	    qthread_gotlock_empty(m[i], src + i);
	}
    }
    /* now all the addresses are either written or queued */
    free(m);			       /* not pooled memory */
    if (X) {
	t->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	/* these are a bit screwey, to save on memory */
	t->blockedon = (struct qthread_lock_s *)X;
	X->waiter = (qthread_t *) queuedlocks;
	qthread_back_to_master(t);
    }
}				       /*}}} */

void qthread_readFE(qthread_t * t, int *dest, int *src)
{				       /*{{{ */
    qthread_readFE_size(t, (char *)dest, (char *)src, sizeof(int));
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

int qthread_lock(qthread_t * t, void *a)
{				       /*{{{ */
    qthread_lock_t *m;


    cp_hashtable_wrlock(qlib->locks);
    m = (qthread_lock_t *) cp_hashtable_get(qlib->locks, a);
    if (m == NULL) {
	if ((m = ALLOC_LOCK(t->shepherd)) == NULL) {
	    perror("qthread_lock()");
	    abort();
	}
	m->waiting = qthread_queue_new(t->shepherd);
	pthread_mutex_init(&m->lock, NULL);
	cp_hashtable_put(qlib->locks, a, m);
	/* since we just created it, we own it */
	assert(pthread_mutex_lock(&m->lock) == 0);
	/* can only unlock the hash after we've locked the address, because
	 * otherwise there's a race condition: the address could be removed
	 * before we have a chance to add ourselves to it */
	cp_hashtable_unlock(qlib->locks);

	m->owner = t->thread_id;
	assert(pthread_mutex_unlock(&m->lock) == 0);
	qthread_debug("qthread_lock(%p, %p): returned (wasn't locked)\n", t,
		      a);
    } else {
	/* success==failure: because it's in the hash, someone else owns the
	 * lock; dequeue this thread and yield.
	 * NOTE: it's up to the master thread to enqueue this thread and unlock
	 * the address
	 */
	assert(pthread_mutex_lock(&m->lock) == 0);
	/* for an explanation of the lock/unlock ordering here, see above */
	cp_hashtable_unlock(qlib->locks);

	t->thread_state = QTHREAD_STATE_BLOCKED;
	t->blockedon = m;

	qthread_back_to_master(t);

	/* once I return to this context, I own the lock! */
	/* conveniently, whoever unlocked me already set up everything too */
	qthread_debug("qthread_lock(%p, %p): returned (was locked)\n", t, a);
    }
    return 1;
}				       /*}}} */

int qthread_unlock(qthread_t * t, void *a)
{				       /*{{{ */
    qthread_lock_t *m;
    qthread_t *u;

    qthread_debug("qthread_unlock(%p, %p): started\n", t, a);

    cp_hashtable_wrlock(qlib->locks);
    m = (qthread_lock_t *) cp_hashtable_get(qlib->locks, a);

    if (m == NULL) {
#if 0
	fprintf(stderr,
		"qthread_unlock(%p,%p): attempt to unlock an address "
		"that is not locked!\n", (void *)t, a);
	abort();
#endif
	return 1;
    }
    assert(pthread_mutex_lock(&m->lock) == 0);

    /* unlock the address... if anybody's waiting for it, give them the lock
     * and put them in a ready queue.  If not, delete the lock structure.
     */

    assert(pthread_mutex_lock(&m->waiting->lock) == 0);

    u = qthread_dequeue_nonblocking(m->waiting);
    if (u == NULL) {
	qthread_debug("qthread_unlock(%p,%p): deleting waiting queue\n", t,
		      a);
	cp_hashtable_remove(qlib->locks, a);
	cp_hashtable_unlock(qlib->locks);

	assert(pthread_mutex_unlock(&m->waiting->lock) == 0);
	qthread_queue_free(m->waiting, t->shepherd);

	assert(pthread_mutex_unlock(&m->lock) == 0);
	assert(pthread_mutex_destroy(&m->lock) == 0);
	FREE(qlib->kthreads[t->shepherd].lock_pool, m);
    } else {
	cp_hashtable_unlock(qlib->locks);
	qthread_debug
	    ("qthread_unlock(%p,%p): pulling thread from queue (%p)\n", t, a,
	     u);
	u->thread_state = QTHREAD_STATE_RUNNING;
	m->owner = u->thread_id;

	/* NOTE: because of the use of getcontext()/setcontext(), threads
	 * return to the shepherd that setcontext()'d into them, so they
	 * must remain in that queue.
	 */
	qthread_enqueue(qlib->kthreads[u->shepherd].ready, u);

	assert(pthread_mutex_unlock(&m->waiting->lock) == 0);
	assert(pthread_mutex_unlock(&m->lock) == 0);
    }

    qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
    return 1;
}				       /*}}} */

/* These are just accessor functions, in case we ever decide to make the qthread_t data structure opaque */
unsigned qthread_id(qthread_t * t)
{				       /*{{{ */
    return t->shepherd;
}				       /*}}} */

void *qthread_arg(qthread_t * t)
{				       /*{{{ */
    return t->arg;
}				       /*}}} */
