#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>		       /* for malloc() and abort() */
#ifndef QTHREAD_NO_ASSERTS
# include <assert.h>		       /* for assert() */
#endif
#if defined(HAVE_UCONTEXT_H) && defined(HAVE_CONTEXT_FUNCS)
# include <ucontext.h>		       /* for make/get/swap-context functions */
#else
# include "osx_compat/taskimpl.h"
#endif
#include <stdarg.h>		       /* for va_start and va_end */
#include <qthread/qthread-int.h>       /* for UINT8_MAX */
#include <string.h>		       /* for memset() */
#if !HAVE_MEMCPY
# define memcpy(d, s, n) bcopy((s), (d), (n))
# define memmove(d, s, n) bcopy((s), (d), (n))
#endif
#ifdef NEED_RLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif
#ifdef QTHREAD_SHEPHERD_PROFILING
# include "qtimer/qtimer.h"
#endif

#include <cprops/mempool.h>
#include <cprops/hashtable.h>
#include <cprops/linked_list.h>

#include "qthread/qthread.h"
#include "qthread/futurelib.h"
#include "qthread_innards.h"
#include "futurelib_innards.h"

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
# ifdef QTHREAD_REALLY_SMALL_STACKS
#  define QTHREAD_DEFAULT_STACK_SIZE 2048
# elif QTHREAD_FULL_SIZE_STACKS
#  define QTHREAD_DEFAULT_STACK_SIZE 4096*2048
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
#define QTHREAD_FUTURE                  1

#ifndef QTHREAD_NOALIGNCHECK
#define ALIGN(d, s, f) do { \
    s = (aligned_t *) (((size_t) d) & MACHINEMASK); \
    if (s != d) { \
	fprintf(stderr, \
		"WARNING: " f ": unaligned address %p ... assuming %p\n", \
		(void *) d, (void *) s); \
    } \
} while(0)
#else /* QTHREAD_NOALIGNCHECK */
#define ALIGN(d, s, f) (s)=(d)
#endif

#ifdef DEBUG_DEADLOCK
#define REPORTLOCK(m) printf("%i:%i LOCKED %p's LOCK!\n", qthread_shep(NULL), __LINE__, m)
#define REPORTUNLOCK(m) printf("%i:%i UNLOCKED %p's LOCK!\n", qthread_shep(NULL), __LINE__, m)
#else
#define REPORTLOCK(m)
#define REPORTUNLOCK(m)
#endif

/* internal data structures */
typedef struct qthread_lock_s qthread_lock_t;
typedef struct qthread_shepherd_s qthread_shepherd_t;
typedef struct qthread_queue_s qthread_queue_t;

struct qthread_s
{
    unsigned int thread_id;
    unsigned char thread_state;
    unsigned char flags;

    /* the shepherd (pthread) we run on */
    qthread_shepherd_t *shepherd_ptr;
    /* the shepherd our memory comes from */
    qthread_shepherd_t *creator_ptr;
    /* a pointer used for passing information back to the shepherd when
     * becoming blocked */
    struct qthread_lock_s *blockedon;

    /* the function to call (that defines this thread) */
    qthread_f f;
    void *arg;			/* user defined data */
    aligned_t *ret;		/* user defined retval location */

    ucontext_t *context;	/* the context switch info */
    void *stack;		/* the thread's stack */
    ucontext_t *return_context;	/* context of parent shepherd */

    struct qthread_s *next;
};

struct qthread_queue_s
{
    qthread_t *head;
    qthread_t *tail;
    qthread_shepherd_t *creator_ptr;
    pthread_mutex_t lock;
    pthread_cond_t notempty;
};

struct qthread_shepherd_s
{
    pthread_t shepherd;
    qthread_shepherd_id_t shepherd_id;	/* whoami */
    qthread_t *current;
    qthread_queue_t *ready;
    cp_mempool *qthread_pool;
    cp_mempool *list_pool;
    cp_mempool *queue_pool;
    cp_mempool *lock_pool;
    cp_mempool *addrres_pool;
    cp_mempool *addrstat_pool;
    cp_mempool *stack_pool;
    cp_mempool *context_pool;
    /* round robin scheduler - can probably be smarter */
    aligned_t sched_shepherd;
#ifdef QTHREAD_SHEPHERD_PROFILING
    double total_time; /* how much time the shepherd spent running */
    double idle_time;		/* how much time the shepherd spent waiting for new threads */
    size_t num_threads; /* number of threads handled */
#endif
};

struct qthread_lock_s
{
    qthread_queue_t *waiting;
    qthread_shepherd_t *creator_ptr;
#ifdef QTHREAD_DEBUG
    unsigned owner;
#endif
    pthread_mutex_t lock;
};

typedef struct qthread_addrres_s
{
    aligned_t *addr;		/* ptr to the memory NOT being blocked on */
    qthread_t *waiter;
    qthread_shepherd_t *creator_ptr;
    struct qthread_addrres_s *next;
} qthread_addrres_t;

typedef struct qthread_addrstat_s
{
    pthread_mutex_t lock;
    qthread_addrres_t *EFQ;
    qthread_addrres_t *FEQ;
    qthread_addrres_t *FFQ;
    qthread_shepherd_t *creator_ptr;
    unsigned int full:1;
} qthread_addrstat_t;

pthread_key_t shepherd_structs;

/* shared globals (w/ futurelib) */
qlib_t qlib = NULL;

/* internal globals */
static cp_mempool *generic_qthread_pool = NULL;
static cp_mempool *generic_stack_pool = NULL;
static cp_mempool *generic_context_pool = NULL;
static cp_mempool *generic_queue_pool = NULL;
static cp_mempool *generic_lock_pool = NULL;
static cp_mempool *generic_addrstat_pool = NULL;

#ifdef QTHREAD_COUNT_THREADS
static aligned_t threadcount = 0;
static pthread_mutex_t threadcount_lock = PTHREAD_MUTEX_INITIALIZER;
static aligned_t maxconcurrentthreads = 0;
static aligned_t concurrentthreads = 0;
static pthread_mutex_t concurrentthreads_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Internal functions */
#ifdef QTHREAD_MAKECONTEXT_SPLIT
static void qthread_wrapper(unsigned int high, unsigned int low);
#else
static void qthread_wrapper(void *ptr);
#endif

static void qthread_FEBlock_delete(qthread_addrstat_t * m);
static inline qthread_t *qthread_thread_new(const qthread_f f,
					    const void *arg, aligned_t * ret,
					    const qthread_shepherd_id_t
					    shepherd);
static inline qthread_t *qthread_thread_bare(const qthread_f f,
					     const void *arg, aligned_t * ret,
					     const qthread_shepherd_id_t
					     shepherd);
static inline void qthread_thread_free(qthread_t * t);
static inline qthread_queue_t *qthread_queue_new(qthread_shepherd_t *
						 shepherd);
static inline void qthread_queue_free(qthread_queue_t * q);
static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t);
static inline qthread_t *qthread_dequeue(qthread_queue_t * q);
static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q);
static inline void qthread_exec(qthread_t * t, ucontext_t * c);
static inline void qthread_back_to_master(qthread_t * t);
static inline void qthread_gotlock_fill(qthread_addrstat_t * m, void *maddr,
					const char recursive);
static inline void qthread_gotlock_empty(qthread_addrstat_t * m, void *maddr,
					 const char recursive);

#define QTHREAD_LOCK(l) qassert(pthread_mutex_lock(l), 0)
#define QTHREAD_UNLOCK(l) qassert(pthread_mutex_unlock(l), 0)
#define QTHREAD_INITLOCK(l) qassert(pthread_mutex_init(l, NULL), 0)
#define QTHREAD_DESTROYLOCK(l) qassert(pthread_mutex_destroy(l), 0)
#define QTHREAD_INITCOND(l) qassert(pthread_cond_init(l, NULL), 0)
#define QTHREAD_DESTROYCOND(l) qassert(pthread_cond_destroy(l), 0)
#define QTHREAD_SIGNAL(l) qassert(pthread_cond_signal(l), 0)
#define QTHREAD_CONDWAIT(c, l) qassert(pthread_cond_wait(c, l), 0)

#if defined(UNPOOLED_QTHREAD_T) || defined(UNPOOLED)
#define ALLOC_QTHREAD(shep) (qthread_t *) malloc(sizeof(qthread_t))
#define FREE_QTHREAD(t) free(t)
#else
static inline qthread_t *ALLOC_QTHREAD(qthread_shepherd_t * shep)
{
    qthread_t *tmp =
	(qthread_t *) cp_mempool_alloc(shep ? (shep->qthread_pool) :
				       generic_qthread_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}
static inline void FREE_QTHREAD(qthread_t * t)
{
    cp_mempool_free(t->
		    creator_ptr ? (t->creator_ptr->
				   qthread_pool) : generic_qthread_pool, t);
}
#endif

#if defined(UNPOOLED_STACKS) || defined(UNPOOLED)
#define ALLOC_STACK(shep) malloc(qlib->qthread_stack_size)
#define FREE_STACK(shep, t) free(t)
#else
#define ALLOC_STACK(shep) cp_mempool_alloc(shep?(shep->stack_pool):generic_stack_pool)
#define FREE_STACK(shep, t) cp_mempool_free(shep?(shep->stack_pool):generic_stack_pool, t)
#endif

#if defined(UNPOOLED_CONTEXTS) || defined(UNPOOLED)
#define ALLOC_CONTEXT(shep) (ucontext_t *) malloc(sizeof(ucontext_t))
#define FREE_CONTEXT(shep, t) free(t)
#else
#define ALLOC_CONTEXT(shep) (ucontext_t *) cp_mempool_alloc(shep?(shep->context_pool):generic_context_pool)
#define FREE_CONTEXT(shep, t) cp_mempool_free(shep?(shep->context_pool):generic_context_pool, t)
#endif

#if defined(UNPOOLED_QUEUES) || defined(UNPOOLED)
#define ALLOC_QUEUE(shep) (qthread_queue_t *) malloc(sizeof(qthread_queue_t))
#define FREE_QUEUE(t) free(t)
#else
static inline qthread_queue_t *ALLOC_QUEUE(qthread_shepherd_t * shep)
{
    qthread_queue_t *tmp =
	(qthread_queue_t *) cp_mempool_alloc(shep ? (shep->queue_pool) :
					     generic_queue_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}
static inline void FREE_QUEUE(qthread_queue_t * t)
{
    cp_mempool_free(t->
		    creator_ptr ? (t->creator_ptr->
				   queue_pool) : generic_queue_pool, t);
}
#endif

#if defined(UNPOOLED_LOCKS) || defined(UNPOOLED)
#define ALLOC_LOCK(shep) (qthread_lock_t *) malloc(sizeof(qthread_lock_t))
#define FREE_LOCK(t) free(t)
#else
static inline qthread_lock_t *ALLOC_LOCK(qthread_shepherd_t * shep)
{
    qthread_lock_t *tmp =
	(qthread_lock_t *) cp_mempool_alloc(shep ? (shep->lock_pool) :
					    generic_lock_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}
static inline void FREE_LOCK(qthread_lock_t * t)
{
    cp_mempool_free(t->
		    creator_ptr ? (t->creator_ptr->
				   lock_pool) : generic_lock_pool, t);
}
#endif

#if defined(UNPOOLED_ADDRRES) || defined(UNPOOLED)
#define ALLOC_ADDRRES(shep) (qthread_addrres_t *) malloc(sizeof(qthread_addrres_t))
#define FREE_ADDRRES(t) free(t)
#else
static inline qthread_addrres_t *ALLOC_ADDRRES(qthread_shepherd_t * shep)
{
    qthread_addrres_t *tmp =
	(qthread_addrres_t *) cp_mempool_alloc(shep->addrres_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}
static inline void FREE_ADDRRES(qthread_addrres_t * t)
{
    cp_mempool_free(t->creator_ptr->addrres_pool, t);
}
#endif

#if defined(UNPOOLED_ADDRSTAT) || defined(UNPOOLED)
#define ALLOC_ADDRSTAT(shep) (qthread_addrstat_t *) malloc(sizeof(qthread_addrstat_t))
#define FREE_ADDRSTAT(t) free(t)
#else
static inline qthread_addrstat_t *ALLOC_ADDRSTAT(qthread_shepherd_t * shep)
{
    qthread_addrstat_t *tmp =
	(qthread_addrstat_t *) cp_mempool_alloc(shep ? (shep->addrstat_pool) :
						generic_addrstat_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}
static inline void FREE_ADDRSTAT(qthread_addrstat_t * t)
{
    cp_mempool_free(t->
		    creator_ptr ? (t->creator_ptr->
				   addrstat_pool) : generic_addrstat_pool, t);
}
#endif


/* guaranteed to be between 0 and 32, using the first parts of addr that are
 * significant */
#define QTHREAD_CHOOSE_BIN(addr) (((size_t)addr >> 4) & 0x1f)

static inline aligned_t qthread_internal_incr(volatile aligned_t * operand,
					      pthread_mutex_t * lock)
{				       /*{{{ */
    aligned_t retval;

#if !defined(QTHREAD_MUTEX_INCREMENT) && ( __PPC__ || _ARCH_PPC || __powerpc__ )
    register unsigned int incrd = incrd;	/* this doesn't need to be initialized */
    asm volatile ("1:\n\t"	/* the tag */
		  "lwarx  %0,0,%1\n\t"	/* reserve operand into retval */
		  "addi   %2,%0,1\n\t"	/* increment */
		  "stwcx. %2,0,%1\n\t"	/* un-reserve operand */
		  "bne-   1b\n\t"	/* if it failed, try again */
		  "isync"	/* make sure it wasn't all a dream */
		  /* = means this operand is write-only (previous value is discarded)
		   * & means this operand is an earlyclobber (i.e. cannot use the same register as any of the input operands)
		   * b means this operand must not be r0 */
		  :"=&b"   (retval)
		  :"r"     (operand), "r"(incrd)
		  :"cc", "memory");
#elif !defined(QTHREAD_MUTEX_INCREMENT) && (defined(__sparc) || defined(__sparc__)) && ! (defined(__SUNPRO_C) || defined(__SUNPRO_CC))
    register aligned_t oldval, newval;

    newval = *operand;
    do {
	retval = oldval = newval;
	newval = oldval + 1;
	/* if (*operand == oldval)
	 * swap(newval, *operand)
	 * else
	 * newval = *operand
	 */
	__asm__ __volatile__("casa [%1] 0x80 , %2, %0":"+r"(newval)
			     :"r"    (operand), "r"(oldval)
			     :"cc", "memory");
    } while (retval != newval);
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ! defined(__INTEL_COMPILER) && ( __ia64 || __ia64__ )
# ifdef __ILP64__
    int64_t res;

    asm volatile ("fetchadd8.rel %0=%1,1":"=r" (res)
		  :"m"     (*operand));

    retval = res;
# else
    int32_t res;

    asm volatile ("fetchadd4.rel %0=%1,1":"=r" (res)
		  :"m"     (*operand));

    retval = res;
# endif
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( __x86_64 || __x86_64__ )
    retval = 1;
    asm volatile ("lock xaddl %0, %1;":"=r" (retval)
		  :"m"     (*operand), "0"(retval));
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( QTHREAD_XEON || __i486 || __i486__ )
    retval = 1;
    asm volatile (".section .smp_locks,\"a\"\n" "  .align 4\n" "  .long 661f\n" ".previous\n" "661:\n\tlock; "	/* this is stolen from the Linux kernel */
		  "xaddl %0, %1":"=r" (retval)
		  :"m"     (*operand), "0"(retval));
#else
#ifndef QTHREAD_MUTEX_INCREMENT
#warning unrecognized architecture: falling back to safe but very slow increment implementation
#endif
    pthread_mutex_lock(lock);
    retval = (*operand)++;
    pthread_mutex_unlock(lock);
#endif
    return retval;
}				       /*}}} */

static inline aligned_t qthread_internal_incr_mod(volatile aligned_t *
						  operand, const int max,
						  pthread_mutex_t * lock)
{				       /*{{{ */
    aligned_t retval;

#if !defined(QTHREAD_MUTEX_INCREMENT) && ( __PPC__ || _ARCH_PPC || __powerpc__ )
    register unsigned int incrd = incrd;	/* these don't need to be initialized */
    register unsigned int compd = compd;	/* they're just tmp variables */

    /* the minus in bne- means "this bne is unlikely to be taken" */
    asm volatile ("1:\n\t"	/* local label */
		  "lwarx  %0,0,%1\n\t"	/* load operand */
		  "addi   %3,%0,1\n\t"	/* increment it into incrd */
		  "cmplw  7,%3,%2\n\t"	/* compare incrd to the max */
		  "mfcr   %4\n\t"	/* move the result into compd */
		  "rlwinm %4,%4,29,1\n\t"	/* isolate the result bit */
		  "mullw  %3,%4,%3\n\t"	/* incrd *= compd */
		  "stwcx. %3,0,%1\n\t"	/* *operand = incrd */
		  "bne-   1b\n\t"	/* if it failed, go to label 1 back */
		  "isync"	/* make sure it wasn't all a dream */
		  /* = means this operand is write-only (previous value is discarded)
		   * & means this operand is an earlyclobber (i.e. cannot use the same register as any of the input operands)
		   * b means this is a register but must not be r0 */
		  :"=&b"   (retval)
		  :"r"     (operand), "r"(max), "r"(incrd), "r"(compd)
		  :"cc", "memory");
#elif !defined(QTHREAD_MUTEX_INCREMENT) && (defined(__sparc) || defined(__sparc__)) && ! (defined(__SUNPRO_C) || defined(__SUNPRO_CC))
    register aligned_t oldval, newval;

    newval = *operand;
    do {
	retval = oldval = newval;
	newval = oldval + 1;
	newval *= (newval < max);
	/* if (*operand == oldval)
	 * swap(newval, *operand)
	 * else
	 * newval = *operand
	 */
	__asm__ __volatile__("casa [%1] 0x80 , %2, %0":"+r"(newval)
			     :"r"    (operand), "r"(oldval)
			     :"cc", "memory");
    } while (retval != newval);
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ! defined(__INTEL_COMPILER) && ( __ia64 || __ia64__ )
# ifdef __ILP64__
    int64_t res, old, new;

    do {
	old = *operand;		       /* atomic, because operand is aligned */
	new = old + 1;
	new *= (new < max);
	asm volatile ("mov ar.ccv=%0;;":	/* no output */
		      :"rO"    (old));

	/* separate so the compiler can insert its junk */
	asm volatile ("cmpxchg8.acq %0=[%1],%2,ar.ccv":"=r" (res)
		      :"r"     (operand), "r"(new)
		      :"memory");
    } while (res != old);	       /* if res==old, new is out of date */
    retval = old;
# else /* 32-bit integers */
    int32_t res, old, new;

    do {
	old = *operand;		       /* atomic, because operand is aligned */
	new = old + 1;
	new *= (new < max);
	asm volatile ("mov ar.ccv=%0;;":	/* no output */
		      :"rO"    (old));

	/* separate so the compiler can insert its junk */
	asm volatile ("cmpxchg4.acq %0=[%1],%2,ar.ccv":"=r" (res)
		      :"r"     (operand), "r"(new)
		      :"memory");
    } while (res != old);	       /* if res==old, new is out of date */
    retval = old;
# endif
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( QTHREAD_XEON || __i486 || __i486__ || __x86_64 || __x86_64__ )
    unsigned long prev;
    unsigned int oldval, newval;

    do {
	oldval = *operand;
	newval = oldval + 1;
	newval *= (newval < max);
	asm volatile ("lock\n\t" "cmpxchgl %1, %2":"=a" (retval)
		      :"r"     (newval), "m"(*operand), "0"(oldval)
		      :"memory");
    } while (retval != oldval);
#else
    pthread_mutex_lock(lock);
    retval = (*operand)++;
    *operand *= (*operand < max);
    pthread_mutex_unlock(lock);
#endif
    return retval;
}				       /*}}} */

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

#define QTHREAD_NONLAZY_THREADIDS
#else
#define qthread_debug(...) do{ }while(0)
#endif

/* the qthread_shepherd() is the pthread responsible for actually
 * executing the work units
 *
 * this function is the workhorse of the library: this is the function that
 * gets spawned several times and runs all the qthreads. */
static void *qthread_shepherd(void *arg)
{				       /*{{{ */
    qthread_shepherd_t *me = (qthread_shepherd_t *) arg;
    ucontext_t my_context;
    qthread_t *t;
    int done = 0;

#ifdef QTHREAD_SHEPHERD_PROFILING
    qtimer_t total = qtimer_new();
    qtimer_t idle = qtimer_new();
    double idletime;
#endif

    qthread_debug("qthread_shepherd(%u): forked\n", me->shepherd_id);

    /* Initialize myself */
    pthread_setspecific(shepherd_structs, arg);
#ifdef QTHREAD_SHEPHERD_PROFILING
    qtimer_start(total);
    me->idle_time = 0.0;
    me->num_threads = 0;
#endif

    /* workhorse loop */
    while (!done) {
#ifdef QTHREAD_SHEPHERD_PROFILING
	qtimer_start(idle);
#endif
	t = qthread_dequeue(me->ready);
#ifdef QTHREAD_SHEPHERD_PROFILING
	qtimer_stop(idle);
	idletime = qtimer_secs(idle);
	me->idle_time += idletime;
#endif

	qthread_debug
	    ("qthread_shepherd(%u): dequeued thread id %d/state %d\n",
	     me->shepherd_id, t->thread_id, t->thread_state);

	if (t->thread_state == QTHREAD_STATE_TERM_SHEP) {
#ifdef QTHREAD_SHEPHERD_PROFILING
	    qtimer_stop(total);
	    me->total_time = qtimer_secs(total);
#endif
	    done = 1;
	    qthread_thread_free(t);
	} else {
	    assert((t->thread_state == QTHREAD_STATE_NEW) ||
		   (t->thread_state == QTHREAD_STATE_RUNNING));

	    assert(t->f != NULL);
#ifdef QTHREAD_SHEPHERD_PROFILING
	    if (t->thread_state == QTHREAD_STATE_NEW) {
		me->num_threads ++;
	    }
#endif

	    assert(t->shepherd_ptr == me);
	    me->current = t;

	    getcontext(&my_context);
	    /* note: there's a good argument that the following should
	     * be: (*t->f)(t), however the state management would be
	     * more complex
	     */
	    qthread_exec(t, &my_context);

	    me->current = NULL;
	    qthread_debug("qthread_shepherd(%u): back from qthread_exec\n",
			  me->shepherd_id);
	    switch (t->thread_state) {
		case QTHREAD_STATE_YIELDED:	/* reschedule it */
		    qthread_debug
			("qthread_shepherd(%u): rescheduling thread %p\n",
			 me->shepherd_id, t);
		    t->thread_state = QTHREAD_STATE_RUNNING;
		    qthread_enqueue(t->shepherd_ptr->ready, t);
		    break;

		case QTHREAD_STATE_FEB_BLOCKED:	/* unlock the related FEB address locks, and re-arrange memory to be correct */
		    qthread_debug
			("qthread_shepherd(%u): unlocking FEB address locks of thread %p\n",
			 me->shepherd_id, t);
		    t->thread_state = QTHREAD_STATE_BLOCKED;
		    QTHREAD_UNLOCK(&
				   (((qthread_addrstat_t *) (t->blockedon))->
				    lock));
		    REPORTUNLOCK(t->blockedon);
		    break;

		case QTHREAD_STATE_BLOCKED:	/* put it in the blocked queue */
		    qthread_debug
			("qthread_shepherd(%u): adding blocked thread %p to blocked queue\n",
			 me->shepherd_id, t);
		    qthread_enqueue((qthread_queue_t *) t->blockedon->waiting,
				    t);
		    QTHREAD_UNLOCK(&(t->blockedon->lock));
		    break;

		case QTHREAD_STATE_TERMINATED:
		    qthread_debug
			("qthread_shepherd(%u): thread %p is in state terminated.\n",
			 me->shepherd_id, t);
		    t->thread_state = QTHREAD_STATE_DONE;
		    /* we can remove the stack and the context... */
		    qthread_thread_free(t);
		    break;
	    }
	}
    }

#ifdef QTHREAD_SHEPHERD_PROFILING
    qtimer_free(total);
    qtimer_free(idle);
#endif
    qthread_debug("qthread_shepherd(%u): finished\n", me->shepherd_id);
    pthread_exit(NULL);
    return NULL;
}				       /*}}} */

int qthread_init(const qthread_shepherd_id_t nshepherds)
{				       /*{{{ */
    int r;
    size_t i;

#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    qthread_debug("qthread_init(): began.\n");

    qlib = (qlib_t) malloc(sizeof(struct qlib_s));
    if (qlib == NULL) {
	return QTHREAD_MALLOC_ERROR;
    }

    /* initialize the FEB-like locking structures */

    /* this is synchronized with read/write locks by default */
    for (i = 0; i < 32; i++) {
#ifdef QTHREAD_COUNT_THREADS
	qlib->locks_stripes[i] = 0;
	qlib->febs_stripes[i] = 0;
# ifdef QTHREAD_MUTEX_INCREMENT
	qassert(pthread_mutex_init(qlib->locks_stripes_locks[i], NULL), 0);
	qassert(pthread_mutex_init(qlib->febs_stripes_locks[i], NULL), 0);
# endif
#endif
	if ((qlib->locks[i] =
	     cp_hashtable_create(10000, cp_hash_addr,
				 cp_hash_compare_addr)) == NULL) {
	    return QTHREAD_MALLOC_ERROR;
	}
	cp_hashtable_set_min_fill_factor(qlib->locks[i], 0);
	if ((qlib->FEBs[i] =
	     cp_hashtable_create_by_option(COLLECTION_MODE_DEEP, 10000,
					   cp_hash_addr, cp_hash_compare_addr,
					   NULL, NULL, NULL, NULL)) == NULL) {
	    return QTHREAD_MALLOC_ERROR;
	}
	cp_hashtable_set_min_fill_factor(qlib->FEBs[i], 0);
    }

    /* initialize the kernel threads and scheduler */
    pthread_key_create(&shepherd_structs, NULL);
    qlib->nshepherds = nshepherds;
    if ((qlib->shepherds = (qthread_shepherd_t *)
	 malloc(sizeof(qthread_shepherd_t) * nshepherds)) == NULL) {
	return QTHREAD_MALLOC_ERROR;
    }

    qlib->qthread_stack_size = QTHREAD_DEFAULT_STACK_SIZE;
    qlib->max_thread_id = 0;
    qlib->sched_shepherd = 0;
    QTHREAD_INITLOCK(&qlib->max_thread_id_lock);
    QTHREAD_INITLOCK(&qlib->sched_shepherd_lock);

#ifdef NEED_RLIMIT
    qassert(getrlimit(RLIMIT_STACK, &rlp), 0);
    qthread_debug("stack sizes ... cur: %u max: %u\n", rlp.rlim_cur,
		  rlp.rlim_max);
    qlib->master_stack_size = rlp.rlim_cur;
    qlib->max_stack_size = rlp.rlim_max;
#endif

    /* set up the memory pools */
    for (i = 0; i < nshepherds; i++) {
	/* the following SHOULD only be accessed by one thread at a time, so
	 * should be quite safe unsynchronized. If things fail, though...
	 * resynchronize them and see if that fixes it. */
	qlib->shepherds[i].qthread_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_t),
					sizeof(qthread_t) * 100);
	qlib->shepherds[i].stack_pool =
	    cp_mempool_create_by_option(0, qlib->qthread_stack_size,
					qlib->qthread_stack_size * 100);
#if ALIGNMENT_PROBLEMS_RETURN
	if (sizeof(ucontext_t) < 2048) {
	    qlib->shepherds[i].context_pool =
		cp_mempool_create_by_option(0, 2048, 2048 * 100);
	} else {
	    qlib->shepherds[i].context_pool =
		cp_mempool_create_by_option(0, sizeof(ucontext_t),
					    sizeof(ucontext_t) * 100);
	}
#else
	qlib->shepherds[i].context_pool =
	    cp_mempool_create_by_option(0, sizeof(ucontext_t),
					sizeof(ucontext_t) * 100);
#endif
	qlib->shepherds[i].list_pool =
	    cp_mempool_create_by_option(0, sizeof(cp_list_entry), 0);
	qlib->shepherds[i].queue_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_queue_t), 0);
	qlib->shepherds[i].lock_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_lock_t), 0);
	qlib->shepherds[i].addrres_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_addrres_t), 0);
	qlib->shepherds[i].addrstat_pool =
	    cp_mempool_create_by_option(0, sizeof(qthread_addrstat_t), 0);
    }
    /* these are used when qthread_fork() is called from a non-qthread. they
     * are protected by a mutex so that things don't get wonky (note: that
     * means qthread_fork is WAY faster if you called it from a qthread) */
    generic_qthread_pool =
	cp_mempool_create_by_option(0, sizeof(qthread_t),
				    sizeof(qthread_t) * 100);
    generic_stack_pool =
	cp_mempool_create_by_option(0, qlib->qthread_stack_size, 0);
    generic_context_pool =
	cp_mempool_create_by_option(0, sizeof(ucontext_t),
				    sizeof(ucontext_t) * 100);
    generic_queue_pool =
	cp_mempool_create_by_option(0, sizeof(qthread_queue_t), 0);
    generic_lock_pool =
	cp_mempool_create_by_option(0, sizeof(qthread_lock_t), 0);
    generic_addrstat_pool =
	cp_mempool_create_by_option(0, sizeof(qthread_addrstat_t), 0);

    /* spawn the number of shepherd threads that were specified */
    for (i = 0; i < nshepherds; i++) {
	qlib->shepherds[i].sched_shepherd = 0;
	qlib->shepherds[i].shepherd_id = i;
	qlib->shepherds[i].ready = qthread_queue_new(NULL);
	if (qlib->shepherds[i].ready == NULL) {
	    perror("qthread_init creating shepherd queue");
	    return QTHREAD_MALLOC_ERROR;
	}

	qthread_debug("qthread_init(): forking shepherd thread %p\n",
		      &qlib->shepherds[i]);

	if ((r =
	     pthread_create(&qlib->shepherds[i].shepherd, NULL,
			    qthread_shepherd, &qlib->shepherds[i])) != 0) {
	    fprintf(stderr, "qthread_init: pthread_create() failed (%d)\n",
		    r);
	    perror("qthread_init spawning shepherd");
	    return r;
	}
    }

    qthread_debug("qthread_init(): finished.\n");
    return QTHREAD_SUCCESS;
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
    for (i = 0; i < qlib->nshepherds; i++) {
	t = qthread_thread_bare(NULL, NULL, (aligned_t *) NULL, i);
	assert(t != NULL);	       /* what else can we do? */
	t->thread_state = QTHREAD_STATE_TERM_SHEP;
	t->thread_id = (unsigned int)-1;
	qthread_enqueue(qlib->shepherds[i].ready, t);
    }

    /* wait for each thread to drain it's queue! */
    for (i = 0; i < qlib->nshepherds; i++) {
	if ((r = pthread_join(qlib->shepherds[i].shepherd, NULL)) != 0) {
	    fprintf(stderr,
		    "qthread_finalize: pthread_join() of shep %i failed (%d)\n",
		    i, r);
	    perror("qthread_finalize");
	    abort();
	}
	qthread_queue_free(qlib->shepherds[i].ready);
#ifdef QTHREAD_SHEPHERD_PROFILING
	printf("Shepherd %i spent %f%% of the time idle (%f:%f) handling %i threads\n", i,
		qlib->shepherds[i].idle_time/qlib->shepherds[i].total_time *
		100.0, qlib->shepherds[i].total_time,
		qlib->shepherds[i].idle_time, qlib->shepherds[i].num_threads);
#endif
    }

    for (i = 0; i < 32; i++) {
	cp_hashtable_destroy(qlib->locks[i]);
	cp_hashtable_destroy_custom(qlib->FEBs[i], NULL, (cp_destructor_fn)
				    qthread_FEBlock_delete);
#ifdef QTHREAD_COUNT_THREADS
	printf("QTHREADS: bin %i used %i/%i times\n", i,
	       qlib->locks_stripes[i], qlib->febs_stripes[i]);
# ifdef QTHREAD_MUTEX_INCREMENT
	QTHREAD_DESTROYLOCK(&qlib->locks_stripes_locks[i]);
	QTHREAD_DESTROYLOCK(&qlib->febs_stripes_locks[i]);
# endif
#endif
    }

#ifdef QTHREAD_COUNT_THREADS
    printf("spawned %lu threads, max concurrency %lu\n",
	   (unsigned long)threadcount, (unsigned long)maxconcurrentthreads);
    QTHREAD_DESTROYLOCK(&threadcount_lock);
    QTHREAD_DESTROYLOCK(&concurrentthreads_lock);
#endif

    QTHREAD_DESTROYLOCK(&qlib->max_thread_id_lock);
    QTHREAD_DESTROYLOCK(&qlib->sched_shepherd_lock);

    for (i = 0; i < qlib->nshepherds; ++i) {
	cp_mempool_destroy(qlib->shepherds[i].qthread_pool);
	cp_mempool_destroy(qlib->shepherds[i].list_pool);
	cp_mempool_destroy(qlib->shepherds[i].queue_pool);
	cp_mempool_destroy(qlib->shepherds[i].lock_pool);
	cp_mempool_destroy(qlib->shepherds[i].addrres_pool);
	cp_mempool_destroy(qlib->shepherds[i].addrstat_pool);
	cp_mempool_destroy(qlib->shepherds[i].stack_pool);
	cp_mempool_destroy(qlib->shepherds[i].context_pool);
    }
    cp_mempool_destroy(generic_qthread_pool);
    cp_mempool_destroy(generic_stack_pool);
    cp_mempool_destroy(generic_context_pool);
    cp_mempool_destroy(generic_queue_pool);
    cp_mempool_destroy(generic_lock_pool);
    cp_mempool_destroy(generic_addrstat_pool);
    free(qlib->shepherds);
    free(qlib);
    qlib = NULL;

    qthread_debug("qthread_finalize(): finished.\n");
}				       /*}}} */

qthread_t *qthread_self(void)
{				       /*{{{ */
    qthread_shepherd_t *shep;

#if 0
    /* size_t mask; */

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
    /* printf("stack pointer should be %p\n", t->stack); */
#endif
    shep = (qthread_shepherd_t *) pthread_getspecific(shepherd_structs);
    return shep ? shep->current : NULL;
}				       /*}}} */

size_t qthread_stackleft(const qthread_t * t)
{				       /*{{{ */
    if (t != NULL) {
	return (size_t) (&t) - (size_t) (t->stack);
    } else {
	return 0;
    }
}				       /*}}} */

aligned_t *qthread_retlock(const qthread_t * t)
{				       /*{{{ */
    if (t) {
	return t->ret;
    } else {
	qthread_t *me = qthread_self();

	if (me) {
	    return me->ret;
	} else {
	    return NULL;
	}
    }
}				       /*}}} */

/************************************************************/
/* functions to manage thread stack allocation/deallocation */
/************************************************************/

static inline qthread_t *qthread_thread_bare(const qthread_f f,
					     const void *arg, aligned_t * ret,
					     const qthread_shepherd_id_t
					     shepherd)
{				       /*{{{ */
    qthread_t *t;

#ifndef UNPOOLED
    t = ALLOC_QTHREAD((&(qlib->shepherds[shepherd])));
#else
    t = ALLOC_QTHREAD(NULL);
#endif
    if (t != NULL) {
#ifdef QTHREAD_NONLAZY_THREADIDS
	/* give the thread an ID number */
	t->thread_id =
	    qthread_internal_incr(&(qlib->max_thread_id),
				  &qlib->max_thread_id_lock);
#else
	t->thread_id = (unsigned int)-1;
#endif
	t->thread_state = QTHREAD_STATE_NEW;
	t->f = f;
	t->arg = (void *)arg;
	t->blockedon = NULL;
	t->shepherd_ptr = &(qlib->shepherds[shepherd]);
	t->ret = ret;
	t->context = NULL;
	t->stack = NULL;
    }
    return t;
}				       /*}}} */

static inline int qthread_thread_plush(qthread_t * t)
{				       /*{{{ */
    ucontext_t *uc;
    void *stack;
    qthread_shepherd_t *shepherd =
	(qthread_shepherd_t *) pthread_getspecific(shepherd_structs);

    uc = ALLOC_CONTEXT(shepherd);
    if (uc != NULL) {
	stack = ALLOC_STACK(shepherd);
	if (stack != NULL) {
	    t->context = uc;
	    t->stack = stack;
	    return QTHREAD_SUCCESS;
	}
	FREE_CONTEXT(shepherd, uc);
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

/* this could be reduced to a qthread_thread_bare() and qthread_thread_plush(),
 * but I *think* doing it this way makes it faster. maybe not, I haven't tested
 * it. */
static inline qthread_t *qthread_thread_new(const qthread_f f,
					    const void *arg, aligned_t * ret,
					    const qthread_shepherd_id_t
					    shepherd)
{				       /*{{{ */
    qthread_t *t;
    ucontext_t *uc;
    void *stack;

#ifndef UNPOOLED
    qthread_shepherd_t *myshep = &(qlib->shepherds[shepherd]);
#else
    qthread_shepherd_t *myshep = NULL;
#endif

    t = ALLOC_QTHREAD(myshep);
    if (t == NULL) {
	return NULL;
    }
    uc = ALLOC_CONTEXT(myshep);
    if (uc == NULL) {
	FREE_QTHREAD(t);
	return NULL;
    }
    stack = ALLOC_STACK(myshep);
    if (stack == NULL) {
	FREE_QTHREAD(t);
	FREE_CONTEXT(myshep, uc);
	return NULL;
    }

    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = (void *)arg;
    t->blockedon = NULL;
    t->shepherd_ptr = &(qlib->shepherds[shepherd]);
    t->ret = ret;
    t->flags = 0;
    t->context = uc;
    t->stack = stack;

#ifdef QTHREAD_NONLAZY_THREADIDS
    /* give the thread an ID number */
    t->thread_id =
	qthread_internal_incr(&(qlib->max_thread_id),
			      &qlib->max_thread_id_lock);
#else
    t->thread_id = (unsigned int)-1;
#endif

    return t;
}				       /*}}} */

static inline void qthread_thread_free(qthread_t * t)
{				       /*{{{ */
    assert(t != NULL);

    if (t->context) {
	FREE_CONTEXT(t->creator_ptr, t->context);
    }
    if (t->stack != NULL) {
	FREE_STACK(t->creator_ptr, t->stack);
    }
    FREE_QTHREAD(t);
}				       /*}}} */


/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

static inline qthread_queue_t *qthread_queue_new(qthread_shepherd_t *
						 shepherd)
{				       /*{{{ */
    qthread_queue_t *q;

    q = ALLOC_QUEUE(shepherd);
    if (q != NULL) {
	q->head = NULL;
	q->tail = NULL;
	QTHREAD_INITLOCK(&q->lock);
	QTHREAD_INITCOND(&q->notempty);
    }
    return q;
}				       /*}}} */

static inline void qthread_queue_free(qthread_queue_t * q)
{				       /*{{{ */
    assert((q->head == NULL) && (q->tail == NULL));
    QTHREAD_DESTROYLOCK(&q->lock);
    QTHREAD_DESTROYCOND(&q->notempty);
    FREE_QUEUE(q);
}				       /*}}} */

static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t)
{				       /*{{{ */
    assert(t != NULL);
    assert(q != NULL);

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

    assert(q->head != NULL);

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
    FREE_ADDRSTAT(m);
}				       /*}}} */

/* this function runs a thread until it completes or yields */
#ifdef QTHREAD_MAKECONTEXT_SPLIT
static void qthread_wrapper(unsigned int high, unsigned int low)
{				       /*{{{ */
    qthread_t *t = (qthread_t *) ((((uintptr_t) high) << 32) | low);
#else
static void qthread_wrapper(void *ptr)
{
    qthread_t *t = (qthread_t *) ptr;
#endif

    qthread_debug("qthread_wrapper(): executing f=%p arg=%p.\n", t->f,
		  t->arg);
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&threadcount, &threadcount_lock);
    qassert(pthread_mutex_lock(&concurrentthreads_lock), 0);
    concurrentthreads++;
    if (concurrentthreads > maxconcurrentthreads)
	maxconcurrentthreads = concurrentthreads;
    qassert(pthread_mutex_unlock(&concurrentthreads_lock), 0);
#endif
    if (t->ret) {
	/* XXX: if this fails, we should probably do something */
	qthread_writeEF_const(t, t->ret, (t->f) (t, t->arg));
    } else {
	(t->f) (t, t->arg);
    }
    t->thread_state = QTHREAD_STATE_TERMINATED;

    qthread_debug("qthread_wrapper(): f=%p arg=%p completed.\n", t->f,
		  t->arg);
#ifdef QTHREAD_COUNT_THREADS
    qassert(pthread_mutex_lock(&concurrentthreads_lock), 0);
    concurrentthreads--;
    qassert(pthread_mutex_unlock(&concurrentthreads_lock), 0);
#endif
    if (t->flags & QTHREAD_FUTURE) {
	future_exit(t);
    }
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

    assert(t != NULL);
    assert(c != NULL);

    if (t->thread_state == QTHREAD_STATE_NEW) {

	qthread_debug("qthread_exec(%p, %p): type is QTHREAD_THREAD_NEW!\n",
		      t, c);
	t->thread_state = QTHREAD_STATE_RUNNING;

	qassert(getcontext(t->context), 0);	/* puts the current context into t->contextq */
	/* Several other libraries that do this reserve a few words on either
	 * end of the stack for some reason. To avoid problems, I'll also do
	 * this (even though I have no idea why they would do this). */
	/* t is cast here ONLY because the PGI compiler is idiotic about typedef's */
#ifdef INVERSE_STACK_POINTER
	t->context->uc_stack.ss_sp =
	    (char *)(((qthread_t *) t)->stack) + qlib->qthread_stack_size - 8;
#else
	t->context->uc_stack.ss_sp = (char *)(((qthread_t *) t)->stack) + 8;
#endif
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
#ifdef QTHREAD_MAKECONTEXT_SPLIT
	{
	    const unsigned int high = ((uintptr_t) t) >> 32;
	    const unsigned int low = ((uintptr_t) t) & 0xffffffff;

#ifdef EXTRA_MAKECONTEXT_ARGC
	    makecontext(t->context, (void (*)(void))qthread_wrapper, 3, high,
			low);
#else
	    makecontext(t->context, (void (*)(void))qthread_wrapper, 2, high,
			low);
#endif
	}
#else
#ifdef EXTRA_MAKECONTEXT_ARGC
	makecontext(t->context, (void (*)(void))qthread_wrapper, 2, t);
#else
	makecontext(t->context, (void (*)(void))qthread_wrapper, 1, t);
#endif
#endif
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
    qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
#endif

    qthread_debug("qthread_exec(%p): executing swapcontext()...\n", t);
    /* return_context (aka "c") is being written over with the current context */
    qassert(swapcontext(t->return_context, t->context), 0);
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_exec(%p): setting stack size limits back to normal...\n",
	 t);
    rlp.rlim_cur = qlib->master_stack_size;
    qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
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

/***********************************************
 * FORKING                                     *
 ***********************************************/
/* fork a thread by putting it in somebody's work queue
 * NOTE: scheduling happens here
 */
int qthread_fork(const qthread_f f, const void *arg, aligned_t * ret)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;
    qthread_shepherd_t *myshep =
	(qthread_shepherd_t *) pthread_getspecific(shepherd_structs);

    if (myshep) {		       /* note: for forking from a qthread, NO LOCKS! */
	shep = myshep->sched_shepherd++;
	if (myshep->sched_shepherd == qlib->nshepherds) {
	    myshep->sched_shepherd = 0;
	}
    } else {
	shep =
	    qthread_internal_incr_mod(&qlib->sched_shepherd, qlib->nshepherds,
				      &qlib->sched_shepherd_lock);
    }
    t = qthread_thread_new(f, arg, ret, shep);
    if (t) {
	qthread_debug("qthread_fork(): tid %u shep %u\n", t->thread_id, shep);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	qthread_enqueue(qlib->shepherds[shep].ready, t);
	return QTHREAD_SUCCESS;
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

int qthread_fork_to(const qthread_f f, const void *arg, aligned_t * ret,
		    const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nshepherds || f == NULL) {
	return QTHREAD_BADARGS;
    }
    t = qthread_thread_new(f, arg, ret, shepherd);
    if (t) {
	qthread_debug("qthread_fork_to(): tid %u shep %u\n", t->thread_id,
		      shepherd);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	qthread_enqueue(qlib->shepherds[shepherd].ready, t);
	return QTHREAD_SUCCESS;
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

int qthread_fork_future_to(const qthread_f f, const void *arg,
			   aligned_t * ret,
			   const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    if (shepherd > qlib->nshepherds) {
	return QTHREAD_BADARGS;
    }
    t = qthread_thread_new(f, arg, ret, shepherd);
    if (t) {
	t->flags |= QTHREAD_FUTURE;
	qthread_debug("qthread_fork_future_to(): tid %u shep %u\n",
		      t->thread_id, shepherd);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	qthread_enqueue(qlib->shepherds[shepherd].ready, t);
	return QTHREAD_SUCCESS;
    }
    return QTHREAD_MALLOC_ERROR;
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
    qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
#endif
    /* now back to your regularly scheduled master thread */
    qassert(swapcontext(t->context, t->return_context), 0);
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_back_to_master(%p): setting stack size limits back to qthread size...\n",
	 t);
    rlp.rlim_cur = qlib->qthread_stack_size;
    qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
#endif
}				       /*}}} */

qthread_t *qthread_prepare(const qthread_f f, const void *arg,
			   aligned_t * ret)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;
    qthread_shepherd_t *myshep =
	(qthread_shepherd_t *) pthread_getspecific(shepherd_structs);

    if (myshep) {
	shep = myshep->sched_shepherd++;
	if (myshep->sched_shepherd == qlib->nshepherds) {
	    myshep->sched_shepherd = 0;
	}
    } else {
	shep =
	    qthread_internal_incr_mod(&qlib->sched_shepherd, qlib->nshepherds,
				      &qlib->sched_shepherd_lock);
    }

    t = qthread_thread_bare(f, arg, ret, shep);
    if (t && ret) {
	if (qthread_empty(qthread_self(), ret) != QTHREAD_SUCCESS) {
	    qthread_thread_free(t);
	    return NULL;
	}
    }
    return t;
}				       /*}}} */

qthread_t *qthread_prepare_for(const qthread_f f, const void *arg,
			       aligned_t * ret,
			       const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t = qthread_thread_bare(f, arg, ret, shepherd);

    if (t && ret) {
	if (qthread_empty(qthread_self(), ret) != QTHREAD_SUCCESS) {
	    qthread_thread_free(t);
	    return NULL;
	}
    }
    return t;
}				       /*}}} */

int qthread_schedule(qthread_t * t)
{				       /*{{{ */
    int ret = qthread_thread_plush(t);

    if (ret == QTHREAD_SUCCESS) {
	qthread_enqueue(t->shepherd_ptr->ready, t);
    }
    return ret;
}				       /*}}} */

int qthread_schedule_on(qthread_t * t, const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    int ret = qthread_thread_plush(t);

    if (ret == QTHREAD_SUCCESS) {
	qthread_enqueue(qlib->shepherds[shepherd].ready, t);
    }
    return ret;
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
    int ret;
    pthread_mutex_t alldone;
};
struct qthread_FEB2_sub_args
{
    const void *src;
    void *dest;
    int ret;
    pthread_mutex_t alldone;
};

/* this one is (strictly-speaking) unnecessary, but I think it helps with
 * optimization to have those consts */
struct qthread_FEB_ef_sub_args
{
    const void *dest;
    int ret;
    pthread_mutex_t alldone;
};

/* This is just a little function that should help in debugging */
int qthread_feb_status(const void *addr)
{				       /*{{{ */
    qthread_addrstat_t *m;
    aligned_t *alignedaddr;
    int status = 1;		/* full */
    const int lockbin = QTHREAD_CHOOSE_BIN(addr);

    ALIGN(addr, alignedaddr, "qthread_feb_status()");
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&qlib->febs_stripes[lockbin],
			  &qlib->febs_stripes_locks[lockbin]);
#endif
    cp_hashtable_rdlock(qlib->FEBs[lockbin]); {
	m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
						    (void *)alignedaddr);
	if (m) {
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	    status = m->full;
	    QTHREAD_UNLOCK(&m->lock);
	    REPORTUNLOCK(m);
	}
    }
    cp_hashtable_unlock(qlib->FEBs[lockbin]);
    return status;
}				       /*}}} */

/* This allocates a new, initialized addrstat structure, which is used for
 * keeping track of the FEB status of an address. It expects a shepherd pointer
 * to use to find the right memory pool to use. */
static inline qthread_addrstat_t *qthread_addrstat_new(qthread_shepherd_t *
						       shepherd)
{				       /*{{{ */
    qthread_addrstat_t *ret = ALLOC_ADDRSTAT(shepherd);

    if (ret != NULL) {
	QTHREAD_INITLOCK(&ret->lock);
	ret->full = 1;
	ret->EFQ = NULL;
	ret->FEQ = NULL;
	ret->FFQ = NULL;
    }
    return ret;
}				       /*}}} */

/* this function removes the FEB data structure for the address maddr from the
 * hash table */
static inline void qthread_FEB_remove(void *maddr)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const int lockbin = QTHREAD_CHOOSE_BIN(maddr);

    qthread_debug("qthread_FEB_remove(): attempting removal\n");
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&qlib->febs_stripes[lockbin],
			  &qlib->febs_stripes_locks[lockbin]);
#endif
    cp_hashtable_wrlock(qlib->FEBs[lockbin]); {
	m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
						    maddr);
	if (m) {
	    QTHREAD_LOCK(&(m->lock));
	    REPORTLOCK(m);
	    if (m->FEQ == NULL && m->EFQ == NULL && m->FFQ == NULL &&
		m->full == 1) {
		qthread_debug
		    ("qthread_FEB_remove(): all lists are empty, and status is full\n");
		cp_hashtable_remove(qlib->FEBs[lockbin], maddr);
	    } else {
		QTHREAD_UNLOCK(&(m->lock));
		REPORTUNLOCK(m);
		qthread_debug
		    ("qthread_FEB_remove(): address cannot be removed; in use\n");
		m = NULL;
	    }
	}
    }
    cp_hashtable_unlock(qlib->FEBs[lockbin]);
    if (m != NULL) {
	QTHREAD_UNLOCK(&m->lock);
	REPORTUNLOCK(m);
	QTHREAD_DESTROYLOCK(&m->lock);
	FREE_ADDRSTAT(m);
    }
}				       /*}}} */

static inline void qthread_gotlock_empty(qthread_addrstat_t * m, void *maddr,
					 const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;

    m->full = 0;
    if (m->EFQ != NULL) {
	/* dQ */
	X = m->EFQ;
	m->EFQ = X->next;
	/* op */
	if (maddr && maddr != X->addr) {
	    memcpy(maddr, X->addr, WORDSIZE);
	}
	m->full = 1;
	/* requeue */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	qthread_enqueue(X->waiter->shepherd_ptr->ready, X->waiter);
	FREE_ADDRRES(X);
	qthread_gotlock_fill(m, maddr, 1);
    }
    if (m->full == 1 && m->EFQ == NULL && m->FEQ == NULL && m->FFQ == NULL)
	removeable = 1;
    else
	removeable = 0;
    if (recursive == 0) {
	QTHREAD_UNLOCK(&m->lock);
	REPORTUNLOCK(m);
	if (removeable) {
	    qthread_FEB_remove(maddr);
	}
    }
}				       /*}}} */

static inline void qthread_gotlock_fill(qthread_addrstat_t * m, void *maddr,
					const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;

    qthread_debug("qthread_gotlock_fill(%p, %p)\n", m, maddr);
    m->full = 1;
    /* dequeue all FFQ, do their operation, and schedule them */
    qthread_debug("qthread_gotlock_fill(): dQ all FFQ\n");
    while (m->FFQ != NULL) {
	/* dQ */
	X = m->FFQ;
	m->FFQ = X->next;
	/* op */
	if (X->addr && X->addr != maddr) {
	    memcpy(X->addr, maddr, WORDSIZE);
	}
	/* schedule */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	qthread_enqueue(X->waiter->shepherd_ptr->ready, X->waiter);
	FREE_ADDRRES(X);
    }
    if (m->FEQ != NULL) {
	/* dequeue one FEQ, do their operation, and schedule them */
	qthread_t *waiter;

	qthread_debug("qthread_gotlock_fill(): dQ 1 FEQ\n");
	X = m->FEQ;
	m->FEQ = X->next;
	/* op */
	if (X->addr && X->addr != maddr) {
	    memcpy(X->addr, maddr, WORDSIZE);
	}
	waiter = X->waiter;
	waiter->thread_state = QTHREAD_STATE_RUNNING;
	m->full = 0;
	qthread_enqueue(waiter->shepherd_ptr->ready, waiter);
	FREE_ADDRRES(X);
	qthread_gotlock_empty(m, maddr, 1);
    }
    if (m->EFQ == NULL && m->FEQ == NULL && m->full == 1)
	removeable = 1;
    else
	removeable = 1;
    if (recursive == 0) {
	QTHREAD_UNLOCK(&m->lock);
	REPORTUNLOCK(m);
	/* now, remove it if it needs to be removed */
	if (removeable) {
	    qthread_FEB_remove(maddr);
	}
    }
}				       /*}}} */

int qthread_empty(qthread_t * me, const void *dest)
{				       /*{{{ */
    qthread_addrstat_t *m;
    aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_BIN(dest);

    ALIGN(dest, alignedaddr, "qthread_empty()");
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&qlib->febs_stripes[lockbin],
			  &qlib->febs_stripes_locks[lockbin]);
#endif
    cp_hashtable_wrlock(qlib->FEBs[lockbin]);
    {				       /* BEGIN CRITICAL SECTION */
	m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
						    (void *)alignedaddr);
	if (!m) {
	    /* currently full, and must be added to the hash to empty */
	    m = qthread_addrstat_new(me ? (me->shepherd_ptr) : NULL);
	    if (!m) {
		cp_hashtable_unlock(qlib->FEBs[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    m->full = 0;
	    cp_hashtable_put(qlib->FEBs[lockbin], (void *)alignedaddr, m);
	    m = NULL;
	} else {
	    /* it could be either full or not, don't know */
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
    }				       /* END CRITICAL SECTION */
    cp_hashtable_unlock(qlib->FEBs[lockbin]);
    if (m) {
	qthread_gotlock_empty(m, (void *)alignedaddr, 0);
    }
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_fill(qthread_t * me, const void *dest)
{				       /*{{{ */
    qthread_addrstat_t *m;
    aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_BIN(dest);

    ALIGN(dest, alignedaddr, "qthread_fill()");
    /* lock hash */
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&qlib->febs_stripes[lockbin],
			  &qlib->febs_stripes_locks[lockbin]);
#endif
    cp_hashtable_wrlock(qlib->FEBs[lockbin]);
    {				       /* BEGIN CRITICAL SECTION */
	m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
						    (void *)alignedaddr);
	if (m) {
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
    }				       /* END CRITICAL SECTION */
    cp_hashtable_unlock(qlib->FEBs[lockbin]);	/* unlock hash */
    if (m) {
	/* if dest wasn't in the hash, it was already full. Since it was,
	 * we need to fill it. */
	qthread_gotlock_fill(m, (void *)alignedaddr, 0);
    }
    return QTHREAD_SUCCESS;
}				       /*}}} */

/* the way this works is that:
 * 1 - data is copies from src to destination
 * 2 - the destination's FEB state gets changed from empty to full
 */

static aligned_t qthread_writeF_sub(qthread_t * me, void *arg)
{				       /*{{{ */
    ((struct qthread_FEB_sub_args *)arg)->ret =
	qthread_writeF(me, ((struct qthread_FEB_sub_args *)arg)->dest,
		       ((struct qthread_FEB_sub_args *)arg)->src);
    pthread_mutex_unlock(&((struct qthread_FEB_sub_args *)arg)->alldone);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_writeF(qthread_t * me, void *dest, const void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	aligned_t *alignedaddr;
	const int lockbin = QTHREAD_CHOOSE_BIN(dest);

	ALIGN(dest, alignedaddr, "qthread_fill_with()");
#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->febs_stripes[lockbin],
			      &qlib->febs_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->FEBs[lockbin]); {	/* lock hash */
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
							(void *)alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd_ptr);
		if (!m) {
		    cp_hashtable_unlock(qlib->FEBs[lockbin]);
		    return QTHREAD_MALLOC_ERROR;
		}
		cp_hashtable_put(qlib->FEBs[lockbin], alignedaddr, m);
	    }
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
	cp_hashtable_unlock(qlib->FEBs[lockbin]);	/* unlock hash */
	/* we have the lock on m, so... */
	if (dest && dest != src) {
	    memcpy(dest, src, WORDSIZE);
	}
	qthread_gotlock_fill(m, alignedaddr, 0);
    } else {
	struct qthread_FEB_sub_args args = { (void *)src, dest, 0 };
	int ret;

	QTHREAD_INITLOCK(&args.alldone);
	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_writeF_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return args.ret;
    }
}				       /*}}} */

int qthread_writeF_const(qthread_t * me, void *dest, const aligned_t src)
{				       /*{{{ */
    return qthread_writeF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state gets changed from empty to full
 */

static aligned_t qthread_writeEF_sub(qthread_t * me, void *arg)
{				       /*{{{ */
    ((struct qthread_FEB_sub_args *)arg)->ret =
	qthread_writeEF(me, ((struct qthread_FEB_sub_args *)arg)->dest,
			((struct qthread_FEB_sub_args *)arg)->src);
    pthread_mutex_unlock(&((struct qthread_FEB_sub_args *)arg)->alldone);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_writeEF(qthread_t * me, void *dest, const void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;
	const int lockbin = QTHREAD_CHOOSE_BIN(dest);

	qthread_debug("qthread_writeEF(%p, %p, %p): init\n", me, dest, src);
	ALIGN(dest, alignedaddr, "qthread_writeEF()");
#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->febs_stripes[lockbin],
			      &qlib->febs_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->FEBs[lockbin]); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
							(void *)alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd_ptr);
		if (!m) {
		    cp_hashtable_unlock(qlib->FEBs[lockbin]);
		    return QTHREAD_MALLOC_ERROR;
		}
		cp_hashtable_put(qlib->FEBs[lockbin], alignedaddr, m);
	    }
	    QTHREAD_LOCK(&(m->lock));
	    REPORTLOCK(m);
	}
	cp_hashtable_unlock(qlib->FEBs[lockbin]);
	qthread_debug("qthread_writeEF(): data structure locked\n");
	/* by this point m is locked */
	qthread_debug("qthread_writeEF(): m->full == %i\n", m->full);
	if (m->full == 1) {	       /* full, thus, we must block */
	    X = ALLOC_ADDRRES(me->shepherd_ptr);
	    if (X == NULL) {
		QTHREAD_UNLOCK(&(m->lock));
		REPORTUNLOCK(m);
		return QTHREAD_MALLOC_ERROR;
	    }
	    X->addr = (aligned_t *) src;
	    X->waiter = me;
	    X->next = m->EFQ;
	    m->EFQ = X;
	} else {
	    if (dest && dest != src) {
		memcpy(dest, src, WORDSIZE);
	    }
	    qthread_gotlock_fill(m, alignedaddr, 0);
	}
	/* now all the addresses are either written or queued */
	qthread_debug("qthread_writeEF(): all written/queued\n");
	if (X) {
	    qthread_debug("qthread_writeEF(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
	return QTHREAD_SUCCESS;
    } else {
	struct qthread_FEB_sub_args args = { (void *)src, dest, 0 };
	int ret;

	QTHREAD_INITLOCK(&args.alldone);
	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_writeEF_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return args.ret;
    }
}				       /*}}} */

int qthread_writeEF_const(qthread_t * me, void *dest, const aligned_t src)
{				       /*{{{ */
    return qthread_writeEF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 */

static aligned_t qthread_readFF_sub(qthread_t * me, void *arg)
{				       /*{{{ */
    ((struct qthread_FEB2_sub_args *)arg)->ret =
	qthread_readFF(me, ((struct qthread_FEB2_sub_args *)arg)->dest,
		       ((struct qthread_FEB2_sub_args *)arg)->src);
    pthread_mutex_unlock(&((struct qthread_FEB2_sub_args *)arg)->alldone);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_readFF(qthread_t * me, void *dest, const void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m = NULL;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;
	const int lockbin = QTHREAD_CHOOSE_BIN(src);

	qthread_debug("qthread_readFF(%p, %p, %p): init\n", me, dest, src);
	ALIGN(src, alignedaddr, "qthread_readFF()");
#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->febs_stripes[lockbin],
			      &qlib->febs_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->FEBs[lockbin]); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
							(void *)alignedaddr);
	    if (!m) {
		if (dest && dest != src) {
		    memcpy(dest, src, WORDSIZE);
		}
	    } else {
		QTHREAD_LOCK(&m->lock);
		REPORTLOCK(m);
	    }
	}
	cp_hashtable_unlock(qlib->FEBs[lockbin]);
	qthread_debug("qthread_readFF(): data structure locked\n");
	/* now m, if it exists, is locked - if m is NULL, then we're done! */
	if (m == NULL)
	    return QTHREAD_SUCCESS;
	if (m->full != 1) {
	    X = ALLOC_ADDRRES(me->shepherd_ptr);
	    if (X == NULL) {
		QTHREAD_UNLOCK(&m->lock);
		REPORTUNLOCK(m);
		return QTHREAD_MALLOC_ERROR;
	    }
	    X->addr = (aligned_t *) dest;
	    X->waiter = me;
	    X->next = m->FFQ;
	    m->FFQ = X;
	} else {
	    if (dest && dest != src) {
		memcpy(dest, src, WORDSIZE);
	    }
	    QTHREAD_UNLOCK(&m->lock);
	    REPORTUNLOCK(m);
	}
	/* if X exists, we are queued, and need to block (i.e. go back to the shepherd) */
	if (X) {
	    qthread_debug("qthread_readFF(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
	return QTHREAD_SUCCESS;
    } else {
	struct qthread_FEB2_sub_args args = { src, dest, 0 };
	int ret;

	QTHREAD_INITLOCK(&args.alldone);
	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_readFF_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return args.ret;
    }
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 * 3 - the src's FEB bits get changed from full to empty
 */

static aligned_t qthread_readFE_sub(qthread_t * me, void *arg)
{				       /*{{{ */
    ((struct qthread_FEB_sub_args *)arg)->ret =
	qthread_readFE(me, ((struct qthread_FEB_sub_args *)arg)->dest,
		       ((struct qthread_FEB_sub_args *)arg)->src);
    pthread_mutex_unlock(&((struct qthread_FEB_sub_args *)arg)->alldone);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_readFE(qthread_t * me, void *dest, void *src)
{				       /*{{{ */
    if (me != NULL) {
	qthread_addrstat_t *m;
	qthread_addrres_t *X = NULL;
	aligned_t *alignedaddr;
	const int lockbin = QTHREAD_CHOOSE_BIN(src);

	qthread_debug("qthread_readFE(%p, %p, %p): init\n", me, dest, src);
	ALIGN(src, alignedaddr, "qthread_readFE()");
#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->febs_stripes[lockbin],
			      &qlib->febs_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->FEBs[lockbin]); {
	    m = (qthread_addrstat_t *) cp_hashtable_get(qlib->FEBs[lockbin],
							alignedaddr);
	    if (!m) {
		m = qthread_addrstat_new(me->shepherd_ptr);
		if (!m) {
		    cp_hashtable_unlock(qlib->FEBs[lockbin]);
		    return QTHREAD_MALLOC_ERROR;
		}
		cp_hashtable_put(qlib->FEBs[lockbin], alignedaddr, m);
	    }
	    QTHREAD_LOCK(&(m->lock));
	    REPORTLOCK(m);
	}
	cp_hashtable_unlock(qlib->FEBs[lockbin]);
	qthread_debug("qthread_readFE(): data structure locked\n");
	/* by this point m is locked */
	if (m->full == 0) {	       /* empty, thus, we must block */
	    X = ALLOC_ADDRRES(me->shepherd_ptr);
	    if (X == NULL) {
		QTHREAD_UNLOCK(&m->lock);
		REPORTUNLOCK(m);
		return QTHREAD_MALLOC_ERROR;
	    }
	    X->addr = (aligned_t *) dest;
	    X->waiter = me;
	    X->next = m->FEQ;
	    m->FEQ = X;
	} else {
	    if (dest && dest != src) {
		memcpy(dest, src, WORDSIZE);
	    }
	    qthread_gotlock_empty(m, alignedaddr, 0);
	}
	/* now all the addresses are either written or queued */
	if (X) {
	    qthread_debug("qthread_readFE(): back to parent\n");
	    me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	    me->blockedon = (struct qthread_lock_s *)m;
	    qthread_back_to_master(me);
	}
	return QTHREAD_SUCCESS;
    } else {
	struct qthread_FEB_sub_args args = { src, dest, 0 };
	int ret;

	QTHREAD_INITLOCK(&args.alldone);
	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_readFE_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return args.ret;
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
    int ret;
    const void *addr;
};

static aligned_t qthread_lock_sub(qthread_t * t, void *arg)
{				       /*{{{ */
    ((struct qthread_lock_sub_args *)arg)->ret =
	qthread_lock(t, ((struct qthread_lock_sub_args *)arg)->addr);
    pthread_mutex_unlock(&(((struct qthread_lock_sub_args *)arg)->alldone));
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_lock(qthread_t * t, const void *a)
{				       /*{{{ */
    qthread_lock_t *m;

    if (t != NULL) {
	const int lockbin = QTHREAD_CHOOSE_BIN(a);

#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->locks_stripes[lockbin],
			      &qlib->locks_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->locks[lockbin]);
	m = (qthread_lock_t *) cp_hashtable_get(qlib->locks[lockbin],
						(void *)a);
	if (m == NULL) {
	    m = ALLOC_LOCK(t->shepherd_ptr);
	    if (m == NULL) {
		cp_hashtable_unlock(qlib->locks[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    /* If we have a shepherd, use it... note that we are ignoring the
	     * qthread_t that got passed in, because that is inherently
	     * untrustworthy. I tested it, actually, and this is faster than
	     * trying to guess whether the qthread_t is accurate or not.
	     */
	    m->waiting = qthread_queue_new(t->shepherd_ptr);
	    if (m->waiting == NULL) {
		FREE_LOCK(m);
		cp_hashtable_unlock(qlib->locks[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    QTHREAD_INITLOCK(&m->lock);
	    cp_hashtable_put(qlib->locks[lockbin], (void *)a, m);
	    /* since we just created it, we own it */
	    QTHREAD_LOCK(&m->lock);
	    /* can only unlock the hash after we've locked the address, because
	     * otherwise there's a race condition: the address could be removed
	     * before we have a chance to add ourselves to it */
	    cp_hashtable_unlock(qlib->locks[lockbin]);

#ifdef QTHREAD_DEBUG
	    m->owner = t->thread_id;
#endif
	    QTHREAD_UNLOCK(&m->lock);
	    qthread_debug("qthread_lock(%p, %p): returned (wasn't locked)\n",
			  t, a);
	} else {
	    /* success==failure: because it's in the hash, someone else owns
	     * the lock; dequeue this thread and yield.
	     * NOTE: it's up to the master thread to enqueue this thread and
	     * unlock the address
	     */
	    QTHREAD_LOCK(&m->lock);
	    /* for an explanation of the lock/unlock ordering here, see above */
	    cp_hashtable_unlock(qlib->locks[lockbin]);

	    t->thread_state = QTHREAD_STATE_BLOCKED;
	    t->blockedon = m;

	    qthread_back_to_master(t);

	    /* once I return to this context, I own the lock! */
	    /* conveniently, whoever unlocked me already set up everything too */
	    qthread_debug("qthread_lock(%p, %p): returned (was locked)\n", t,
			  a);
	}
	return QTHREAD_SUCCESS;
    } else {
	struct qthread_lock_sub_args args =
	    { PTHREAD_MUTEX_INITIALIZER, 0, a };
	int ret;

	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_lock_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	return args.ret;
    }
}				       /*}}} */

static aligned_t qthread_unlock_sub(qthread_t * t, void *arg)
{				       /*{{{ */
    ((struct qthread_lock_sub_args *)arg)->ret =
	qthread_unlock(t, ((struct qthread_lock_sub_args *)arg)->addr);
    pthread_mutex_unlock(&(((struct qthread_lock_sub_args *)arg)->alldone));
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_unlock(qthread_t * t, const void *a)
{				       /*{{{ */
    qthread_lock_t *m;
    qthread_t *u;

    qthread_debug("qthread_unlock(%p, %p): started\n", t, a);

    if (t != NULL) {
	const int lockbin = QTHREAD_CHOOSE_BIN(a);

#ifdef QTHREAD_COUNT_THREADS
	qthread_internal_incr(&qlib->locks_stripes[lockbin],
			      &qlib->locks_stripes_locks[lockbin]);
#endif
	cp_hashtable_wrlock(qlib->locks[lockbin]);
	m = (qthread_lock_t *) cp_hashtable_get(qlib->locks[lockbin],
						(void *)a);
	if (m == NULL) {
	    /* unlocking an address that's already locked */
	    cp_hashtable_unlock(qlib->locks[lockbin]);
	    return QTHREAD_REDUNDANT;
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
	    cp_hashtable_remove(qlib->locks[lockbin], (void *)a);
	    cp_hashtable_unlock(qlib->locks[lockbin]);

	    QTHREAD_UNLOCK(&m->waiting->lock);
	    qthread_queue_free(m->waiting);
	    QTHREAD_UNLOCK(&m->lock);
	    QTHREAD_DESTROYLOCK(&m->lock);
	    FREE_LOCK(m);
	} else {
	    cp_hashtable_unlock(qlib->locks[lockbin]);
	    qthread_debug
		("qthread_unlock(%p,%p): pulling thread from queue (%p)\n", t,
		 a, u);
	    u->thread_state = QTHREAD_STATE_RUNNING;
#ifdef QTHREAD_DEBUG
	    m->owner = u->thread_id;
#endif

	    /* NOTE: because of the use of getcontext()/setcontext(), threads
	     * return to the shepherd that setcontext()'d into them, so they
	     * must remain in that queue.
	     */
	    qthread_enqueue(u->shepherd_ptr->ready, u);

	    QTHREAD_UNLOCK(&m->waiting->lock);
	    QTHREAD_UNLOCK(&m->lock);
	}

	qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
	return QTHREAD_SUCCESS;
    } else {
	struct qthread_lock_sub_args args =
	    { PTHREAD_MUTEX_INITIALIZER, 0, a };
	int ret;

	QTHREAD_LOCK(&args.alldone);
	ret = qthread_fork(qthread_unlock_sub, &args, NULL);
	if (ret == QTHREAD_SUCCESS) {
	    QTHREAD_LOCK(&args.alldone);
	} else {
	    args.ret = ret;
	}
	QTHREAD_UNLOCK(&args.alldone);
	QTHREAD_DESTROYLOCK(&args.alldone);
	qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
	return args.ret;
    }
}				       /*}}} */

/* These are just accessor functions */
unsigned qthread_id(const qthread_t * t)
{				       /*{{{ */
#ifdef QTHREAD_NONLAZY_THREADIDS
    return t ? t->thread_id : (unsigned int)-1;
#else
    if (!t) {
	return (unsigned int)-1;
    }
    if (t->thread_id != (unsigned int)-1) {
	return t->thread_id;
    }
    ((qthread_t *) t)->thread_id =
	qthread_internal_incr(&(qlib->max_thread_id),
			      &qlib->max_thread_id_lock);
    return t->thread_id;
#endif
}				       /*}}} */

qthread_shepherd_id_t qthread_shep(const qthread_t * t)
{				       /*{{{ */
    qthread_shepherd_t *ret;

    if (t) {
	return t->shepherd_ptr->shepherd_id;
    }
    ret = (qthread_shepherd_t *) pthread_getspecific(shepherd_structs);
    if (ret == NULL) {
	return NO_SHEPHERD;
    } else {
	return ret->shepherd_id;
    }
}				       /*}}} */

/* these two functions are helper functions for futurelib
 * (nobody else gets to have 'em!) */
unsigned int qthread_isfuture(const qthread_t * t)
{				       /*{{{ */
    return t ? (t->flags & QTHREAD_FUTURE) : 0;
}				       /*}}} */

void qthread_assertfuture(qthread_t * t)
{				       /*{{{ */
    t->flags |= QTHREAD_FUTURE;
}				       /*}}} */

void qthread_assertnotfuture(qthread_t * t)
{				       /*{{{ */
    t->flags &= ~QTHREAD_FUTURE;
}				       /*}}} */
