#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>		       /* for malloc() and abort() */
#ifdef HAVE_MALLOC_H
# include <malloc.h>		       /* for memalign() */
#endif
#if defined(HAVE_UCONTEXT_H) && defined(HAVE_NATIVE_MAKECONTEXT)
# include <ucontext.h>		       /* for make/get/swap-context functions */
#else
# include "osx_compat/taskimpl.h"
#endif
#include <qthread/qthread-int.h>       /* for UINT8_MAX */
#include <string.h>		       /* for memset() */
#if !HAVE_MEMCPY
# define memcpy(d, s, n) bcopy((s), (d), (n))
# define memmove(d, s, n) bcopy((s), (d), (n))
#endif
#include <sys/time.h>
#include <sys/resource.h>
#if (defined(QTHREAD_SHEPHERD_PROFILING) || defined(QTHREAD_LOCK_PROFILING))
# include "qtimer.h"
#endif
#ifdef QTHREAD_USE_PTHREADS
# include <pthread.h>
#endif
#ifdef HAVE_SCHED_H
# include <sched.h>
#endif
#ifdef QTHREAD_USE_VALGRIND
# include <valgrind/memcheck.h>
#endif
#ifdef QTHREAD_GUARD_PAGES
#include <sys/types.h>
#include <sys/mman.h>
#endif

#ifdef QTHREAD_USE_PLPA
#include <plpa.h>
#endif
#ifdef HAVE_PROCESSOR_BIND
# include <sys/types.h>
# include <sys/processor.h>
# include <sys/procset.h>
# ifdef HAVE_SYS_LGRP_USER_H
#  include <sys/lgrp_user.h>
# endif
#endif
#ifdef QTHREAD_HAVE_LIBNUMA
# include <numa.h>
#endif
#ifdef HAVE_SYSCTL
# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# ifdef HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
# endif
#endif
#if defined(HAVE_SYSCONF) && ! defined(QTHREAD_HAVE_MACHTOPO) && defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#ifdef HAVE_MACH_THREAD_POLICY_H
# include <mach/mach_init.h>
# include <mach/thread_policy.h>
kern_return_t thread_policy_set(thread_t thread,
				thread_policy_flavor_t flavor,
				thread_policy_t policy_info,
				mach_msg_type_number_t count);
kern_return_t thread_policy_get(thread_t thread,
				thread_policy_flavor_t flavor,
				thread_policy_t policy_info,
				mach_msg_type_number_t * count,
				boolean_t * get_default);
#endif

#include "qt_mpool.h"
#include "qt_atomics.h"

#include "qthread/qthread.h"
#include "qthread/futurelib.h"
#include "qthread_innards.h"
#include "futurelib_innards.h"

/* internal constants */
enum threadstate {
    QTHREAD_STATE_NEW,
    QTHREAD_STATE_RUNNING,
    QTHREAD_STATE_YIELDED,
    QTHREAD_STATE_BLOCKED,
    QTHREAD_STATE_FEB_BLOCKED,
    QTHREAD_STATE_TERMINATED,
    QTHREAD_STATE_DONE,
    QTHREAD_STATE_MIGRATING,
    QTHREAD_STATE_TERM_SHEP = UINT8_MAX };
/* flags (must be different bits) */
#define QTHREAD_FUTURE                  1
#define QTHREAD_REAL_MCCOY		2

#ifndef QTHREAD_NOALIGNCHECK
#define QALIGN(d, s, f) do { \
    s = (aligned_t *) (((size_t) d) & (~(sizeof(aligned_t)-1))); \
    if (s != d) { \
	fprintf(stderr, \
		"WARNING: " f ": unaligned address %p ... assuming %p\n", \
		(void *) d, (void *) s); \
    } \
} while(0)
#else /* QTHREAD_NOALIGNCHECK */
#define QALIGN(d, s, f) (s)=(d)
#endif

#if !(defined(HAVE_GCC_INLINE_ASSEMBLY) && \
    (QTHREAD_SIZEOF_ALIGNED_T == 4 || \
     (QTHREAD_ASSEMBLY_ARCH != QTHREAD_POWERPC32 && \
      QTHREAD_ASSEMBLY_ARCH != QTHREAD_SPARCV9_32))) && \
    ! defined(QTHREAD_MUTEX_INCREMENT)
# warn QTHREAD_MUTEX_INCREMENT not defined. It probably should be.
# define QTHREAD_MUTEX_INCREMENT 1
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
    enum threadstate thread_state;
    unsigned char flags;

    /* the shepherd we run on */
    qthread_shepherd_t *shepherd_ptr;
    /* the shepherd we'd rather run on */
    qthread_shepherd_t *target_shepherd;
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

#ifdef QTHREAD_USE_VALGRIND
    unsigned int valgrind_stack_id;
#endif

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

/* queue declarations */
typedef struct _qt_lfqueue_node
{
    qthread_t *value;
#ifdef QTHREAD_MUTEX_INCREMENT
    struct _qt_lfqueue_node *next;
#else
    volatile struct _qt_lfqueue_node *volatile next;
#endif
    qthread_shepherd_t *creator_ptr;
} qt_lfqueue_node_t;

typedef struct qt_lfqueue_s
{
#ifdef QTHREAD_MUTEX_INCREMENT
    qt_lfqueue_node_t *head;
    qt_lfqueue_node_t *tail;
    QTHREAD_FASTLOCK_TYPE head_lock;
    QTHREAD_FASTLOCK_TYPE tail_lock;
    QTHREAD_FASTLOCK_TYPE advisory_queuelen_m;
#else
    volatile qt_lfqueue_node_t *volatile head;
    volatile qt_lfqueue_node_t *volatile tail;
#ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
    volatile aligned_t fruitless;
    pthread_mutex_t lock;
    pthread_cond_t notempty;
#endif /* CONDWAIT */
#endif /* MUTEX_INCREMENT */
    /* the following is for estimating a queue's "busy" level, and is not
     * guaranteed accurate (that would be a race condition) */
    volatile saligned_t advisory_queuelen;
    qthread_shepherd_t *creator_ptr;
} qt_lfqueue_t;

struct qthread_shepherd_s
{
    pthread_t shepherd;
    qthread_shepherd_id_t shepherd_id;	/* whoami */
    qthread_t *current;
    qt_lfqueue_t *ready;
    /* memory pools */
    qt_mpool qthread_pool;
    qt_mpool queue_pool;
    qt_mpool lfqueue_pool;
    qt_mpool lfqueue_node_pool;
    qt_mpool lock_pool;
    qt_mpool addrres_pool;
    qt_mpool addrstat_pool;
    qt_mpool stack_pool;
    qt_mpool context_pool;
    /* round robin scheduler - can probably be smarter */
    aligned_t sched_shepherd;
    volatile uintptr_t QTHREAD_CASLOCK(active);
    /* affinity information */
    unsigned int node;		/* whereami */
#ifdef QTHREAD_HAVE_LGRP
    unsigned int lgrp;
#endif
    unsigned int *shep_dists;
    qthread_shepherd_id_t *sorted_sheplist;
#ifdef QTHREAD_SHEPHERD_PROFILING
    double total_time;		/* how much time the shepherd spent running */
    double idle_maxtime;	/* max time the shepherd spent waiting for new threads */
    double idle_time;		/* how much time the shepherd spent waiting for new threads */
    size_t idle_count;		/* how many times the shepherd did a blocking dequeue */
    size_t num_threads;		/* number of threads handled */
#endif
#ifdef QTHREAD_LOCK_PROFILING
    qt_hash uniquelockaddrs;	/* the unique addresses that are locked */
    double aquirelock_maxtime;	/* max time spent aquiring locks */
    double aquirelock_time;	/* total time spent aquiring locks */
    size_t aquirelock_count;	/* num locks aquired */
    double lockwait_maxtime;	/* max time spent blocked on a lock */
    double lockwait_time;	/* total time spent blocked on a lock */
    size_t lockwait_count;	/* num times blocked on a lock */
    double hold_maxtime;	/* max time spent holding locks */
    double hold_time;		/* total time spent holding locks (use aquirelock_count) */

    qt_hash uniquefebaddrs;	/* unique addresses that are associated with febs */
    double febblock_maxtime;	/* max time spent aquiring FEB words */
    double febblock_time;	/* total time spent aquiring FEB words */
    size_t febblock_count;	/* num FEB words aquired */
    double febwait_maxtime;	/* max time spent blocking on FEBs */
    double febwait_time;	/* total time spent blocking on FEBs */
    size_t febwait_count;	/* num FEB blocking waits required */
    double empty_maxtime;	/* max time addresses spent empty */
    double empty_time;		/* total time addresses spent empty */
    size_t empty_count;		/* num times addresses were empty */
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
#ifdef QTHREAD_LOCK_PROFILING
    qtimer_t hold_timer;
#endif
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
#ifdef QTHREAD_LOCK_PROFILING
    qtimer_t empty_timer;
#endif
    unsigned int full:1;
} qthread_addrstat_t;

pthread_key_t shepherd_structs;

/* shared globals (w/ futurelib) */
qlib_t qlib = NULL;
int qaffinity = 1;

#ifdef QTHREAD_COUNT_THREADS
static aligned_t threadcount = 0;
static pthread_mutex_t threadcount_lock = PTHREAD_MUTEX_INITIALIZER;
static aligned_t maxconcurrentthreads = 0;
static aligned_t concurrentthreads = 0;
static pthread_mutex_t concurrentthreads_lock = PTHREAD_MUTEX_INITIALIZER;

#define QTHREAD_COUNT_THREADS_BINCOUNTER(TYPE, BIN) qthread_internal_incr(&qlib->TYPE##_stripes[(BIN)], &qlib->TYPE##_stripes_locks[(BIN)], 1)
#else
#define QTHREAD_COUNT_THREADS_BINCOUNTER(TYPE, BIN) do{ }while(0)
#endif

/* Internal functions */
#ifdef QTHREAD_MAKECONTEXT_SPLIT
static void qthread_wrapper(unsigned int high, unsigned int low);
#else
static void qthread_wrapper(void *ptr);
#endif

static QINLINE void qthread_makecontext(ucontext_t * const, void * const, const size_t,
					void (*)(void), const void * const,
					ucontext_t * const);
static QINLINE qthread_addrstat_t *qthread_addrstat_new(qthread_shepherd_t *
							shepherd);
static void qthread_addrstat_delete(qthread_addrstat_t * m);
static QINLINE qthread_t *qthread_thread_new(const qthread_f f,
					     const void *arg, aligned_t * ret,
					     const qthread_shepherd_id_t
					     shepherd);
static QINLINE qthread_t *qthread_thread_bare(const qthread_f f,
					      const void *arg,
					      aligned_t * ret,
					      const qthread_shepherd_id_t
					      shepherd);
static QINLINE void qthread_thread_free(qthread_t * t);
static qthread_shepherd_t* qthread_find_active_shepherd(qthread_shepherd_id_t *l, unsigned int *d);

static QINLINE qt_lfqueue_t *qt_lfqueue_new(qthread_shepherd_t * shepherd);
static QINLINE void qt_lfqueue_free(qt_lfqueue_t * q);
static QINLINE void qt_lfqueue_enqueue(qt_lfqueue_t * q, qthread_t * t,
                                      qthread_shepherd_t * shep);
static QINLINE qthread_t *qt_lfqueue_dequeue(qt_lfqueue_t * q);
static QINLINE qthread_t *qt_lfqueue_dequeue_blocking(qt_lfqueue_t * q);
static QINLINE qthread_queue_t *qthread_queue_new(qthread_shepherd_t *
 				  shepherd);
static QINLINE void qthread_queue_free(qthread_queue_t * q);
static QINLINE void qthread_enqueue(qthread_queue_t * q, qthread_t * t);

/*static QINLINE qthread_t *qthread_dequeue(qthread_queue_t * q);*/
static QINLINE qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q);

static QINLINE void qthread_exec(qthread_t * t, ucontext_t * c);
static QINLINE void qthread_back_to_master(qthread_t * t);
static QINLINE void qthread_gotlock_fill(qthread_shepherd_t * shep,
					 qthread_addrstat_t * m, void *maddr,
					 const char recursive);
static QINLINE void qthread_gotlock_empty(qthread_shepherd_t * shep,
					  qthread_addrstat_t * m, void *maddr,
					  const char recursive);
#if (defined(QTHREAD_HAVE_LIBNUMA) || defined(QTHREAD_HAVE_LGRP))
#ifdef HAVE_QSORT_R
# if !defined(__linux__)
static int qthread_internal_shepcomp(void *src, const void *a, const void *b)
# else
static int qthread_internal_shepcomp(const void *a, const void *b, void *src)
#endif
{
    int a_dist =
	qthread_distance((qthread_shepherd_id_t) (intptr_t) src,
			 *(qthread_shepherd_id_t *) a);
    int b_dist =
	qthread_distance((qthread_shepherd_id_t) (intptr_t) src,
			 *(qthread_shepherd_id_t *) b);
    return a_dist - b_dist;
}
#else
static qthread_shepherd_id_t shepcomp_src;

/* this cannot be static, because Sun's idiotic gccfss compiler sometimes (at
 * optimization levels > -O3) refuses to compile it if it is - note that this
 * doesn't seem to be something that can be detected with a configure script,
 * because it WORKS on small programs */
int qthread_internal_shepcomp(const void *a, const void *b)
{
    int a_dist = qthread_distance(shepcomp_src, *(qthread_shepherd_id_t *) a);
    int b_dist = qthread_distance(shepcomp_src, *(qthread_shepherd_id_t *) b);

    return a_dist - b_dist;
}
#endif
#endif

#define QTHREAD_INITLOCK(l) do { if (pthread_mutex_init(l, NULL) != 0) { return QTHREAD_PTHREAD_ERROR; } } while(0)
#define QTHREAD_LOCK(l) qassert(pthread_mutex_lock(l), 0)
#define QTHREAD_UNLOCK(l) qassert(pthread_mutex_unlock(l), 0)
#define QTHREAD_DESTROYLOCK(l) qassert(pthread_mutex_destroy(l), 0)
#define QTHREAD_DESTROYCOND(l) qassert(pthread_cond_destroy(l), 0)
#define QTHREAD_SIGNAL(l) qassert(pthread_cond_signal(l), 0)
#define QTHREAD_CONDWAIT(c, l) qassert(pthread_cond_wait(c, l), 0)

#if defined(UNPOOLED_QTHREAD_T) || defined(UNPOOLED)
#define ALLOC_QTHREAD(shep) (qthread_t *) malloc(sizeof(qthread_t))
#define FREE_QTHREAD(t) free(t)
#else
static qt_mpool generic_qthread_pool = NULL;
static QINLINE qthread_t *ALLOC_QTHREAD(qthread_shepherd_t * shep)
{
    qthread_t *tmp =
	(qthread_t *) qt_mpool_alloc(shep ? (shep->qthread_pool) :
				     generic_qthread_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_QTHREAD(qthread_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 qthread_pool) : generic_qthread_pool, t);
}
#endif

#if defined(UNPOOLED_STACKS) || defined(UNPOOLED)
# ifdef QTHREAD_GUARD_PAGES
static QINLINE void *ALLOC_STACK(qthread_shepherd_t * shep)
{
    char *tmp = valloc(qlib->qthread_stack_size + (2 * getpagesize()));

    if (mprotect(tmp, getpagesize(), PROT_NONE) != 0) {
	perror("mprotect alloc");
    }
    mprotect(tmp + qlib->qthread_stack_size + getpagesize(), getpagesize(),
	     PROT_NONE);
    return tmp + getpagesize();
}
static QINLINE void FREE_STACK(qthread_shepherd_t * shep, void *t)
{
    char *tmp = t;

    tmp -= getpagesize();
    if (mprotect(tmp, getpagesize(), PROT_READ | PROT_WRITE) != 0) {
	perror("mprotect free");
    }
    mprotect(tmp + qlib->qthread_stack_size + getpagesize(), getpagesize(),
	     PROT_READ | PROT_WRITE);
    free(tmp);
}
# else
#  define ALLOC_STACK(shep) malloc(qlib->qthread_stack_size)
#  define FREE_STACK(shep, t) free(t)
# endif
#else
static qt_mpool generic_stack_pool = NULL;
# ifdef QTHREAD_GUARD_PAGES
static QINLINE void *ALLOC_STACK(qthread_shepherd_t * shep)
{
    char *tmp =
	qt_mpool_alloc(shep ? (shep->stack_pool) : generic_stack_pool);
    if (mprotect(tmp, getpagesize(), PROT_NONE) != 0) {
	perror("mprotect alloc");
    }
    mprotect(tmp + qlib->qthread_stack_size + getpagesize(), getpagesize(),
	     PROT_NONE);
    return tmp + getpagesize();
}
static QINLINE void FREE_STACK(qthread_shepherd_t * shep, void *t)
{
    char *tmp = t;

    tmp -= getpagesize();
    if (mprotect(tmp, getpagesize(), PROT_READ | PROT_WRITE) != 0) {
	perror("mprotect free");
    }
    mprotect(tmp + qlib->qthread_stack_size + getpagesize(), getpagesize(),
	     PROT_READ | PROT_WRITE);
    qt_mpool_free(shep ? (shep->stack_pool) : generic_stack_pool, tmp);
}
# else
#  define ALLOC_STACK(shep) qt_mpool_alloc(shep?(shep->stack_pool):generic_stack_pool)
#  define FREE_STACK(shep, t) qt_mpool_free(shep?(shep->stack_pool):generic_stack_pool, t)
# endif
#endif

#if defined(UNPOOLED_CONTEXTS) || defined(UNPOOLED)
#define ALLOC_CONTEXT(shep) (ucontext_t *) calloc(1, sizeof(ucontext_t))
#define FREE_CONTEXT(shep, t) free(t)
#else
static qt_mpool generic_context_pool = NULL;
#define ALLOC_CONTEXT(shep) (ucontext_t *) qt_mpool_alloc(shep?(shep->context_pool):generic_context_pool)
#define FREE_CONTEXT(shep, t) qt_mpool_free(shep?(shep->context_pool):generic_context_pool, t)
#endif

#if defined(UNPOOLED_QUEUES) || defined(UNPOOLED)
# define ALLOC_QUEUE(shep) (qthread_queue_t *) malloc(sizeof(qthread_queue_t))
# define FREE_QUEUE(t) free(t)
# define ALLOC_LFQUEUE(shep) (qt_lfqueue_t *) malloc(sizeof(qt_lfqueue_t))
# define FREE_LFQUEUE(t) free(t)
static QINLINE void ALLOC_LFQNODE(qt_lfqueue_node_t ** ret,
				  qthread_shepherd_t * shep)
{
# ifdef HAVE_POSIX_MEMALIGN
    qassert(posix_memalign((void **)ret, 16, sizeof(qt_lfqueue_node_t)), 0);
# elif HAVE_MEMALIGN
    *ret = (qt_lfqueue_node_t *) memalign(16, sizeof(qt_lfqueue_node_t));
# else
    *ret = calloc(1, sizeof(qt_lfqueue_node_t));
    return;
# endif
    memset(*ret, 0, sizeof(qt_lfqueue_node_t));
}

# define FREE_LFQNODE(t) free(t)
#else
static qt_mpool generic_queue_pool = NULL;
static qt_mpool generic_lfqueue_pool = NULL;
static qt_mpool generic_lfqueue_node_pool = NULL;
static QINLINE qthread_queue_t *ALLOC_QUEUE(qthread_shepherd_t * shep)
{
    qthread_queue_t *tmp =
	(qthread_queue_t *) qt_mpool_alloc(shep ? (shep->queue_pool) :
					   generic_queue_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_QUEUE(qthread_queue_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 queue_pool) : generic_queue_pool, t);
}

static QINLINE qt_lfqueue_t *ALLOC_LFQUEUE(qthread_shepherd_t * shep)
{
    qt_lfqueue_t *tmp =
	(qt_lfqueue_t *) qt_mpool_alloc(shep ? (shep->lfqueue_pool) :
					generic_lfqueue_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_LFQUEUE(qt_lfqueue_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 lfqueue_pool) : generic_lfqueue_pool, t);
}

static QINLINE void ALLOC_LFQNODE(qt_lfqueue_node_t ** ret,
				  qthread_shepherd_t * shep)
{
    *ret =
	(qt_lfqueue_node_t *) qt_mpool_alloc(shep ? (shep->lfqueue_node_pool)
					     : generic_lfqueue_node_pool);
    if (*ret != NULL) {
	memset(*ret, 0, sizeof(qt_lfqueue_node_t));
	(*ret)->creator_ptr = shep;
    }
}

static QINLINE void FREE_LFQNODE(qt_lfqueue_node_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 lfqueue_node_pool) :
		  generic_lfqueue_node_pool, t);
}
#endif

#if defined(UNPOOLED_LOCKS) || defined(UNPOOLED)
#define ALLOC_LOCK(shep) (qthread_lock_t *) malloc(sizeof(qthread_lock_t))
#define FREE_LOCK(t) free(t)
#else
static qt_mpool generic_lock_pool = NULL;
static QINLINE qthread_lock_t *ALLOC_LOCK(qthread_shepherd_t * shep)
{
    qthread_lock_t *tmp =
	(qthread_lock_t *) qt_mpool_alloc(shep ? (shep->lock_pool) :
					  generic_lock_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_LOCK(qthread_lock_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 lock_pool) : generic_lock_pool, t);
}
#endif

#if defined(UNPOOLED_ADDRRES) || defined(UNPOOLED)
#define ALLOC_ADDRRES(shep) (qthread_addrres_t *) malloc(sizeof(qthread_addrres_t))
#define FREE_ADDRRES(t) free(t)
#else
static QINLINE qthread_addrres_t *ALLOC_ADDRRES(qthread_shepherd_t * shep)
{
    qthread_addrres_t *tmp =
	(qthread_addrres_t *) qt_mpool_alloc(shep->addrres_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_ADDRRES(qthread_addrres_t * t)
{
    qt_mpool_free(t->creator_ptr->addrres_pool, t);
}
#endif

#if defined(UNPOOLED_ADDRSTAT) || defined(UNPOOLED)
#define ALLOC_ADDRSTAT(shep) (qthread_addrstat_t *) malloc(sizeof(qthread_addrstat_t))
#define FREE_ADDRSTAT(t) free(t)
#else
static qt_mpool generic_addrstat_pool = NULL;
static QINLINE qthread_addrstat_t *ALLOC_ADDRSTAT(qthread_shepherd_t * shep)
{
    qthread_addrstat_t *tmp =
	(qthread_addrstat_t *) qt_mpool_alloc(shep ? (shep->addrstat_pool) :
					      generic_addrstat_pool);
    if (tmp != NULL) {
	tmp->creator_ptr = shep;
    }
    return tmp;
}

static QINLINE void FREE_ADDRSTAT(qthread_addrstat_t * t)
{
    qt_mpool_free(t->
		  creator_ptr ? (t->creator_ptr->
				 addrstat_pool) : generic_addrstat_pool, t);
}
#endif


/* guaranteed to be between 0 and 128, using the first parts of addr that are
 * significant */
#ifdef __SUN__
#define QTHREAD_CHOOSE_STRIPE(addr) (((size_t)addr >> 4) & 0x7f)
#define QTHREAD_LOCKING_STRIPES 128
#else
#define QTHREAD_CHOOSE_STRIPE(addr) (((size_t)addr >> 4) & 0x1f)
#define QTHREAD_LOCKING_STRIPES 32
#endif

#if !defined(QTHREAD_MUTEX_INCREMENT)
#define qthread_internal_atomic_read_s(op,lock) (*op)
#define qthread_internal_incr(op,lock,val) qthread_incr(op, val)
#define qthread_internal_incr_s(op,lock,val) qthread_incr(op, val)
#define qthread_internal_decr(op,lock) qthread_incr(op, -1)
#define qthread_internal_incr_mod(op,m,lock) qthread_internal_incr_mod_(op,m)
#define QTHREAD_OPTIONAL_LOCKARG
#else
#define qthread_internal_incr_mod(op,m,lock) qthread_internal_incr_mod_(op,m,lock)
#define QTHREAD_OPTIONAL_LOCKARG , QTHREAD_FASTLOCK_TYPE *lock
static QINLINE aligned_t qthread_internal_incr(volatile aligned_t * operand,
					       QTHREAD_FASTLOCK_TYPE * lock,
					       int val)
{				       /*{{{ */
    aligned_t retval;

    QTHREAD_FASTLOCK_LOCK(lock);
    retval = *operand;
    *operand += val;
    QTHREAD_FASTLOCK_UNLOCK(lock);
    return retval;
}				       /*}}} */
static QINLINE saligned_t qthread_internal_incr_s(volatile saligned_t *
						  operand,
						  QTHREAD_FASTLOCK_TYPE * lock,
						  int val)
{				       /*{{{ */
    saligned_t retval;

    QTHREAD_FASTLOCK_LOCK(lock);
    retval = *operand;
    *operand += val;
    QTHREAD_FASTLOCK_UNLOCK(lock);
    return retval;
}				       /*}}} */

static QINLINE saligned_t qthread_internal_atomic_read_s(volatile saligned_t *
							 operand,
							 QTHREAD_FASTLOCK_TYPE *
							 lock)
{				       /*{{{ */
    saligned_t retval;

    QTHREAD_FASTLOCK_LOCK(lock);
    retval = *operand;
    QTHREAD_FASTLOCK_UNLOCK(lock);
    return retval;
}				       /*}}} */
#endif

static QINLINE aligned_t qthread_internal_incr_mod_(volatile aligned_t *
						    operand,
						    const int max
						    QTHREAD_OPTIONAL_LOCKARG)
{				       /*{{{ */
    aligned_t retval;

#if defined(HAVE_GCC_INLINE_ASSEMBLY)

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
    ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64) && (QTHREAD_SIZEOF_ALIGNED_T == 4))

    register unsigned int incrd = incrd;	/* these don't need to be initialized */
    register unsigned int compd = compd;	/* they're just tmp variables */

    /* the minus in bne- means "this bne is unlikely to be taken" */
    asm volatile ("1:\n\t"	/* local label */
		  "lwarx  %0,0,%3\n\t"	/* load operand */
		  "addi   %2,%0,1\n\t"	/* increment it into incrd */
		  "cmplw  7,%2,%4\n\t"	/* compare incrd to the max */
		  "mfcr   %1\n\t"	/* move the result into compd */
		  "rlwinm %1,%1,29,1\n\t"	/* isolate the result bit */
		  "mullw  %2,%2,%1\n\t"	/* incrd *= compd */
		  "stwcx. %2,0,%3\n\t"	/* *operand = incrd */
		  "bne-   1b\n\t"	/* if it failed, go to label 1 back */
		  "isync"	/* make sure it wasn't all a dream */
		  /* = means this operand is write-only (previous value is discarded)
		   * & means this operand is an earlyclobber (i.e. cannot use the same register as any of the input operands)
		   * b means this is a register but must not be r0 */
		  :"=&b"   (retval), "=&r"(compd), "=&r"(incrd)
		  :"r"     (operand), "r"(max)
		  :"cc", "memory");

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)

    register uint64_t incrd = incrd;
    register uint64_t compd = compd;

    asm volatile ("1:\n\t"	/* local label */
		  "ldarx  %0,0,%3\n\t"	/* load operand */
		  "addi   %2,%0,1\n\t"	/* increment it into incrd */
		  "cmpl   7,1,%2,%4\n\t"	/* compare incrd to the max */
		  "mfcr   %1\n\t"	/* move the result into compd */
		  "rlwinm %1,%1,29,1\n\t"	/* isolate the result bit */
		  "mulld  %2,%2,%1\n\t"	/* incrd *= compd */
		  "stdcx. %2,0,%3\n\t"	/* *operand = incrd */
		  "bne-   1b\n\t"	/* if it failed, to to label 1 back */
		  "isync"	/* make sure it wasn't all just a dream */
		  :"=&b"   (retval), "=&r"(compd), "=&r"(incrd)
		  :"r"     (operand), "r"(max)
		  :"cc", "memory");

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32) || \
      ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64) && (QTHREAD_SIZEOF_ALIGNED_T == 4))

    register uint32_t oldval, newval;

    /* newval = *operand; */
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
	retval = oldval = *operand;
	newval = oldval + 1;
	newval *= (newval < max);
	/* if (*operand == oldval)
	 * swap(newval, *operand)
	 * else
	 * newval = *operand
	 */
	__asm__ __volatile__("cas [%1] , %2, %0"	/* */
			     :"=&r"  (newval)
			     :"r"    (operand), "r"(oldval), "0"(newval)
			     :"memory");
    } while (oldval != newval);

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)

    register aligned_t oldval, newval;

    /* newval = *operand; */
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
	retval = oldval = *operand;
	newval = oldval + 1;
	newval *= (newval < max);
	/* if (*operand == oldval)
	 * swap(newval, *operand)
	 * else
	 * newval = *operand
	 */
	__asm__ __volatile__("casx [%1] , %2, %0":"=&r"(newval)
			     :"r"    (operand), "r"(oldval)
#if !defined(__SUNPRO_CC) && !defined(__SUNPRO_C)
			     , "0"(newval)
#endif
			     :"memory");
    } while (oldval != newval);

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)

# if QTHREAD_SIZEOF_ALIGNED_T == 8

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

# else /* 32-bit aligned_t */

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

#elif ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) && (QTHREAD_SIZEOF_ALIGNED_T == 4)) || \
      ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) && (QTHREAD_SIZEOF_ALIGNED_T == 4))

    unsigned int oldval, newval;

    do {
	oldval = *operand;
	newval = oldval + 1;
	newval *= (newval < max);
	asm volatile ("lock; cmpxchgl %1, (%2)":"=&a" (retval)
		      :"r"     (newval), "r"(operand), "0"(oldval)
		      :"memory");
    } while (retval != oldval);

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)

    union
    {
	uint64_t i;
	struct
	{
	    /* note: the ordering of these is important and counter-intuitive; welcome to little-endian! */
	    uint32_t l;
	    uint32_t h;
	} s;
    } oldval, newval;
    register char test;

    do {
# ifdef __PIC__
	/* this saves off %ebx to make PIC code happy :P */
#  define QTHREAD_PIC_PREFIX "pushl %%ebx\n\tmovl %4, %%ebx\n\t"
	/* this restores it */
#  define QTHREAD_PIC_SUFFIX "\n\tpopl %%ebx"
#  define QTHREAD_PIC_REG "m"
# else
#  define QTHREAD_PIC_PREFIX
#  define QTHREAD_PIC_SUFFIX
#  define QTHREAD_PIC_REG "b"
# endif
	oldval.i = *operand;
	newval.i = oldval.i + 1;
	newval.i *= (newval.i < max);
	__asm__ __volatile__(QTHREAD_PIC_PREFIX "lock; cmpxchg8b (%1)\n\t" "setne %0"	/* test = (ZF==0) */
			     QTHREAD_PIC_SUFFIX:"=q"(test)
			     :"r"    (operand), /*EAX*/ "a"(oldval.s.l),
			     /*EDX*/ "d"(oldval.s.h),
			     /*EBX*/ QTHREAD_PIC_REG(newval.s.l),
			     /*ECX*/ "c"(newval.s.h)
			     :"memory");
    } while (test);
    retval = oldval.i;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)

    unsigned long oldval, newval;

    do {
	oldval = *operand;
	newval = oldval + 1;
	newval *= (newval < max);
	asm volatile ("lock; cmpxchgq %1, (%2)":"=a" (retval)
		      :"r"     (newval), "r"(operand), "0"(oldval)
		      :"memory");
    } while (retval != oldval);

#else

#error "Unimplemented assembly architecture"

#endif

#elif defined(QTHREAD_MUTEX_INCREMENT)

    QTHREAD_FASTLOCK_LOCK(lock);
    retval = (*operand)++;
    *operand *= (*operand < max);
    QTHREAD_FASTLOCK_UNLOCK(lock);

#else

#error "Neither atomic or mutex increment enabled"

#endif

    return retval;
}				       /*}}} */

/* to avoid compiler bugs regarding volatile... */
#ifndef QTHREAD_MUTEX_INREMENT
static Q_NOINLINE volatile qt_lfqueue_node_t *volatile *vol_id_qtlfqn(volatile
								      qt_lfqueue_node_t
								      *
								      volatile
								      *ptr)
{
    return ptr;
}
# ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
static Q_NOINLINE aligned_t vol_read_a(volatile aligned_t * ptr)
{
    return *ptr;
}
static Q_NOINLINE volatile aligned_t *vol_id_a(volatile aligned_t * ptr)
{
    return ptr;
}
# endif
# define _(x) *vol_id_qtlfqn(&(x))
#endif

#ifdef QTHREAD_DEBUG
enum qthread_debug_levels debuglevel = 0;
QTHREAD_FASTLOCK_TYPE output_lock;

int qthread_debuglevel(int d)
{
    if (d >= 0) debuglevel = d;
    return debuglevel;
}
#else
int qthread_debuglevel(int d)
{
    return 0;
}
#endif

#ifdef QTHREAD_LOCK_PROFILING
# define QTHREAD_ACCUM_MAX(a, b) do { if ((a) < (b)) { a = b; } } while (0)
# define QTHREAD_WAIT_TIMER_DECLARATION qtimer_t wait_timer = qtimer_new();
# define QTHREAD_WAIT_TIMER_START() qtimer_start(wait_timer)
# define QTHREAD_WAIT_TIMER_STOP(ME, TYPE) do { double secs; \
    qtimer_stop(wait_timer); \
    secs = qtimer_secs(wait_timer); \
    if ((ME)->shepherd_ptr->TYPE##_maxtime < secs) { \
	(ME)->shepherd_ptr->TYPE##_maxtime = secs; } \
    (ME)->shepherd_ptr->TYPE##_time += secs; \
    (ME)->shepherd_ptr->TYPE##_count ++; \
    qtimer_free(wait_timer); } while(0)
# define QTHREAD_LOCK_TIMER_DECLARATION(TYPE) qtimer_t TYPE##_timer = qtimer_new();
# define QTHREAD_LOCK_TIMER_START(TYPE) qtimer_start(TYPE##_timer)
# define QTHREAD_LOCK_TIMER_STOP(TYPE, ME) do { double secs; \
    qtimer_stop(TYPE##_timer); \
    secs = qtimer_secs(TYPE##_timer); \
    if ((ME)->shepherd_ptr->TYPE##_maxtime < secs) { \
	(ME)->shepherd_ptr->TYPE##_maxtime = secs; } \
    (ME)->shepherd_ptr->TYPE##_time += qtimer_secs(TYPE##_timer); \
    (ME)->shepherd_ptr->TYPE##_count ++; \
    qtimer_free(TYPE##_timer); } while(0)
# define QTHREAD_HOLD_TIMER_INIT(LOCKSTRUCT_P) (LOCKSTRUCT_P)->hold_timer = qtimer_new()
# define QTHREAD_HOLD_TIMER_START(LOCKSTRUCT_P) qtimer_start((LOCKSTRUCT_P)->hold_timer)
# define QTHREAD_HOLD_TIMER_STOP(LOCKSTRUCT_P, SHEP) do { double secs; \
    qtimer_stop((LOCKSTRUCT_P)->hold_timer); \
    secs = qtimer_secs((LOCKSTRUCT_P)->hold_timer); \
    if ((SHEP)->hold_maxtime < secs) { \
	(SHEP)->hold_maxtime = secs; } \
    (SHEP)->hold_time += secs; } while(0)
# define QTHREAD_HOLD_TIMER_DESTROY(LOCKSTRUCT_P) qtimer_free((LOCKSTRUCT_P)->hold_timer)
# define QTHREAD_EMPTY_TIMER_INIT(LOCKSTRUCT_P) (LOCKSTRUCT_P)->empty_timer = qtimer_new()
# define QTHREAD_EMPTY_TIMER_START(LOCKSTRUCT_P) qtimer_start((LOCKSTRUCT_P)->empty_timer)
# define QTHREAD_EMPTY_TIMER_STOP(LOCKSTRUCT_P) do { qthread_shepherd_t *ret; \
    double secs; \
    qtimer_stop((LOCKSTRUCT_P)->empty_timer); \
    ret = pthread_getspecific(shepherd_structs); \
    assert(ret != NULL); \
    secs = qtimer_secs((LOCKSTRUCT_P)->empty_timer); \
    if (ret->empty_maxtime < secs) { \
	ret->empty_maxtime = secs; } \
    ret->empty_time += secs; \
    ret->empty_count ++; } while (0)
# define QTHREAD_LOCK_UNIQUERECORD(TYPE, ADDR, ME) qt_hash_put((ME)->shepherd_ptr->unique##TYPE##addrs, (void*)(ADDR), (void*)(ADDR))
static QINLINE int qthread_unique_collect(void *key, void *value, void *id)
{
    qt_hash_put((qt_hash) id, key, value);
    return 0;
}
#else
# define QTHREAD_WAIT_TIMER_DECLARATION
# define QTHREAD_WAIT_TIMER_START() do{ }while(0)
# define QTHREAD_WAIT_TIMER_STOP(ME, TYPE) do{ }while(0)
# define QTHREAD_LOCK_TIMER_DECLARATION(TYPE)
# define QTHREAD_LOCK_TIMER_START(TYPE) do{ }while(0)
# define QTHREAD_LOCK_TIMER_STOP(TYPE, ME) do{ }while(0)
# define QTHREAD_HOLD_TIMER_INIT(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_HOLD_TIMER_START(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_HOLD_TIMER_STOP(LOCKSTRUCT_P, SHEP) do{ }while(0)
# define QTHREAD_HOLD_TIMER_DESTROY(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_EMPTY_TIMER_INIT(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_EMPTY_TIMER_START(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_EMPTY_TIMER_STOP(LOCKSTRUCT_P) do{ }while(0)
# define QTHREAD_LOCK_UNIQUERECORD(TYPE, ADDR, ME) do{ }while(0)
#endif

#ifdef QTHREAD_HAVE_LGRP
static int lgrp_walk(const lgrp_cookie_t cookie, const lgrp_id_t lgrp,
		     processorid_t ** cpus, lgrp_id_t * lgrp_ids,
		     int cpu_grps)
{				       /*{{{ */
    int nchildren, ncpus =
	lgrp_cpus(cookie, lgrp, NULL, 0, LGRP_CONTENT_DIRECT);

    if (ncpus == -1) {
	return cpu_grps;
    } else if (ncpus > 0) {
	processorid_t *cpuids = malloc((ncpus + 1) * sizeof(processorid_t));

	ncpus = lgrp_cpus(cookie, lgrp, cpuids, ncpus, LGRP_CONTENT_DIRECT);
	if (ncpus == -1) {
	    free(cpuids);
	    return cpu_grps;
	}
	cpuids[ncpus] = -1;
	cpus[cpu_grps] = cpuids;
	lgrp_ids[cpu_grps] = lgrp;
	cpu_grps++;
    }
    nchildren = lgrp_children(cookie, lgrp, NULL, 0);
    if (nchildren == -1) {
	return cpu_grps;
    } else if (nchildren > 0) {
	int i;
	lgrp_id_t *children = malloc(nchildren * sizeof(lgrp_id_t));

	nchildren = lgrp_children(cookie, lgrp, children, nchildren);
	if (nchildren == -1) {
	    free(children);
	    return cpu_grps;
	}
	for (i = 0; i < nchildren; i++) {
	    cpu_grps =
		lgrp_walk(cookie, children[i], cpus, lgrp_ids, cpu_grps);
	}
    }
    return cpu_grps;
}				       /*}}} */
#endif

#ifndef QTHREAD_NO_ASSERTS
void * shep0arg = NULL;
#endif
/* the qthread_shepherd() is the pthread responsible for actually
 * executing the work units
 *
 * this function is the workhorse of the library: this is the function that
 * gets spawned several times and runs all the qthreads. */
#ifdef QTHREAD_MAKECONTEXT_SPLIT
static void *qthread_shepherd(void *arg);
static void *qthread_shepherd_wrapper(unsigned int high, unsigned int low)
{				       /*{{{ */
    qthread_shepherd_t *me =
	(qthread_shepherd_t *) ((((uintptr_t) high) << 32) | low);
    qthread_debug(ALL_DETAILS, "qthread_shepherd_wrapper(%x, %x): me = %p\n",
		  high, low, me);
    return qthread_shepherd(me);
}
#endif
static void *qthread_shepherd(void *arg)
{
    qthread_shepherd_t *me = (qthread_shepherd_t *) arg;
    ucontext_t my_context;
    qthread_t *t;
    int done = 0;

#ifdef QTHREAD_SHEPHERD_PROFILING
    qtimer_t total = qtimer_new();
    qtimer_t idle = qtimer_new();
#endif

    qthread_debug(ALL_DETAILS, "qthread_shepherds(): alive! me = %p\n", me);
    assert(me != NULL);
    assert(me->shepherd_id <= qlib->nshepherds);
    qthread_debug(ALL_FUNCTIONS, "qthread_shepherd(%u): forked with arg %p\n",
		  me->shepherd_id, arg);
#ifndef QTHREAD_NO_ASSERTS
    if (shep0arg != NULL && me->shepherd_id == 0) {
	if (arg != shep0arg) {
	    fprintf(stderr, "arg = %p, shep0arg = %p\n", arg, shep0arg);
	}
	assert(arg == shep0arg);
	shep0arg = NULL;
    }
#endif

    /* Initialize myself */
    pthread_setspecific(shepherd_structs, arg);
    if (qaffinity) {		       /*{{{ */
#ifdef QTHREAD_HAVE_MACHTOPO
	mach_msg_type_number_t Count = THREAD_AFFINITY_POLICY_COUNT;
	thread_affinity_policy_data_t mask[THREAD_AFFINITY_POLICY_COUNT];

	/*
	 * boolean_t GetDefault = 0;
	 * if (thread_policy_get(mach_thread_self(),
	 * THREAD_AFFINITY_POLICY,
	 * (thread_policy_t)&mask,
	 * &Count,
	 * &GetDefault) != KERN_SUCCESS) {
	 * printf("ERROR! Cannot get affinity for some reason\n");
	 * }
	 * printf("THREAD_AFFINITY_POLICY: krc=%#x default=%d\n",
	 * krc, GetDefault);
	 * printf("\tcount=%i\n", Count);
	 * for (int i=0; i<Count; i++) {
	 * printf("\t\taffinity_tag=%d (%#x)\n",
	 * mask[i].affinity_tag, mask[i].affinity_tag);
	 * } */
	memset(mask, 0,
	       sizeof(thread_affinity_policy_data_t) *
	       THREAD_AFFINITY_POLICY_COUNT);
	mask[0].affinity_tag = me->shepherd_id + 1;
	Count = 1;
	if (thread_policy_set
	    (mach_thread_self(), THREAD_AFFINITY_POLICY,
	     (thread_policy_t) & mask, Count) != KERN_SUCCESS) {
	    fprintf(stderr, "ERROR! Cannot SET affinity for some reason\n");
	}
#elif defined(QTHREAD_HAVE_LIBNUMA)
	if (numa_run_on_node(me->node) != 0) {
	    numa_error("setting thread affinity");
	}
	numa_set_preferred(me->node);
#elif defined(QTHREAD_USE_PLPA)
	plpa_cpu_set_t *cpuset =
	    (plpa_cpu_set_t *) malloc(sizeof(plpa_cpu_set_t));
	PLPA_CPU_ZERO(cpuset);
	PLPA_CPU_SET(me->shepherd_id, cpuset);
	if (plpa_sched_setaffinity(0, sizeof(plpa_cpu_set_t), cpuset) < 0 &&
	    errno != EINVAL) {
	    perror("plpa setaffinity");
	}
	free(cpuset);
#elif defined(QTHREAD_HAVE_LGRP)
	if (me->node != -1) {
	    if (lgrp_affinity_set(P_LWPID, P_MYID, me->lgrp, LGRP_AFF_STRONG)
		!= 0) {
		perror("lgrp_affinity_set");
	    }
	}
#elif defined(HAVE_PROCESSOR_BIND)
	if (me->node != -1) {
	    if (processor_bind(P_LWPID, P_MYID, me->node, NULL) < 0) {
		perror("processor_bind");
	    }
	}
#endif
    }
    /*}}} */

    /* workhorse loop */
    while (!done) {
#ifdef QTHREAD_SHEPHERD_PROFILING
	qtimer_start(idle);
#endif
	qthread_debug(ALL_DETAILS, "qthread_shepherd(%i): waiting for my queue...\n", me->shepherd_id);
	t = qt_lfqueue_dequeue_blocking(me->ready);
#ifdef QTHREAD_SHEPHERD_PROFILING
	qtimer_stop(idle);
	me->idle_count++;
	me->idle_time += qtimer_secs(idle);
	if (me->idle_maxtime < qtimer_secs(idle)) {
	    me->idle_maxtime = qtimer_secs(idle);
	}
#endif

	qthread_debug(THREAD_DETAILS,
		      "qthread_shepherd(%u): dequeued thread id %d/state %d\n",
		      me->shepherd_id, t->thread_id, t->thread_state);

	if (t->thread_state == QTHREAD_STATE_TERM_SHEP) {
#ifdef QTHREAD_SHEPHERD_PROFILING
	    qtimer_stop(total);
	    me->total_time = qtimer_secs(total);
#endif
	    done = 1;
	    qthread_thread_free(t);
	} else {
	    /* yielded only happens for the first thread */
	    assert((t->thread_state == QTHREAD_STATE_NEW) ||
		   (t->thread_state == QTHREAD_STATE_RUNNING) ||
		   (t->thread_state == QTHREAD_STATE_YIELDED &&
		    t->flags & QTHREAD_REAL_MCCOY));

	    assert(t->f != NULL || t->flags & QTHREAD_REAL_MCCOY);
	    assert(t->shepherd_ptr == me);

	    if (t->target_shepherd != NULL && t->target_shepherd != me &&
		QTHREAD_CASLOCK_READ_UI(t->target_shepherd->active)) {
		/* send this thread home */
		qthread_debug(THREAD_DETAILS,
			      "qthread_shepherd(%u): thread %u going back home to shep %u\n",
			      me->shepherd_id, t->thread_id,
			      t->target_shepherd->shepherd_id);
		t->shepherd_ptr = t->target_shepherd;
		qt_lfqueue_enqueue(t->shepherd_ptr->ready, t, me);
	    } else if (!QTHREAD_CASLOCK_READ_UI(me->active)) {
		qthread_debug(ALL_DETAILS,
			      "qthread_shepherd(%u): skipping thread exec because I've been disabled!\n",
			      me->shepherd_id);
		if (t->target_shepherd == NULL || t->target_shepherd == me) {
		    /* send to the closest shepherd */
		    t->shepherd_ptr =
			qthread_find_active_shepherd(me->sorted_sheplist,
						     me->shep_dists);
		} else {
		    /* find a shepherd somewhere near the preferred shepherd
		     *
		     * Note: if the preferred shep was active, we would have sent
		     * this thread home above */
		    t->shepherd_ptr =
			qthread_find_active_shepherd(t->target_shepherd->
						     sorted_sheplist,
						     t->target_shepherd->
						     shep_dists);
		}
		assert(t->shepherd_ptr);
		if (t->shepherd_ptr == NULL) {
		    qthread_debug(THREAD_DETAILS,
				  "qthread_shepherd(%u): a new home for thread %i could not be found!\n",
				  me->shepherd_id, t->thread_id);
		    t->shepherd_ptr = me;
		}
		qthread_debug(THREAD_DETAILS,
			      "qthread_shepherd(%u): rescheduling thread %i on %i\n",
			      me->shepherd_id, t->thread_id,
			      t->shepherd_ptr->shepherd_id);
		qt_lfqueue_enqueue(t->shepherd_ptr->ready, t, me);
	    } else {		       /* me->active */
#ifdef QTHREAD_SHEPHERD_PROFILING
		if (t->thread_state == QTHREAD_STATE_NEW) {
		    me->num_threads++;
		}
#endif
		me->current = t;
		getcontext(&my_context);
		/* note: there's a good argument that the following should
		 * be: (*t->f)(t), however the state management would be
		 * more complex
		 */
		qthread_exec(t, &my_context);
		me->current = NULL;
		qthread_debug(ALL_DETAILS,
			      "qthread_shepherd(%u): back from qthread_exec\n",
			      me->shepherd_id);
		/* now clean up, based on the thread's state */
		switch (t->thread_state) {
		    case QTHREAD_STATE_MIGRATING:
			qthread_debug(THREAD_DETAILS,
				      "qthread_shepherd(%u): thread %u migrating to shep %u\n",
				      me->shepherd_id, t->thread_id,
				      t->target_shepherd->shepherd_id);
			t->thread_state = QTHREAD_STATE_RUNNING;
			t->shepherd_ptr = t->target_shepherd;
			qt_lfqueue_enqueue(t->shepherd_ptr->ready, t, me);
			break;
		    default:
			qthread_debug(THREAD_DETAILS, "qthread_shepherd(%u): thread in state %i; that's illegal!\n", me->shepherd_id, t->thread_state);
			assert(0);
		    case QTHREAD_STATE_YIELDED:	/* reschedule it */
			t->thread_state = QTHREAD_STATE_RUNNING;
			qthread_debug(THREAD_DETAILS,
				      "qthread_shepherd(%u): thread %i yielded; rescheduling\n",
				      me->shepherd_id, t->thread_id);
			qt_lfqueue_enqueue(me->ready, t, me);
			break;

		    case QTHREAD_STATE_FEB_BLOCKED:	/* unlock the related FEB address locks, and re-arrange memory to be correct */
			qthread_debug(LOCK_DETAILS,
				      "qthread_shepherd(%u): thread %i blocked on FEB\n",
				      me->shepherd_id, t->thread_id);
			t->thread_state = QTHREAD_STATE_BLOCKED;
			QTHREAD_UNLOCK(&
				       (((qthread_addrstat_t *) (t->
								 blockedon))->
					lock));
			REPORTUNLOCK(t->blockedon);
			break;

		    case QTHREAD_STATE_BLOCKED:	/* put it in the blocked queue */
			qthread_debug(LOCK_DETAILS,
				      "qthread_shepherd(%u): thread %i blocked on LOCK\n",
				      me->shepherd_id, t->thread_id);
			qthread_enqueue((qthread_queue_t *) t->blockedon->
					waiting, t);
			QTHREAD_UNLOCK(&(t->blockedon->lock));
			break;

		    case QTHREAD_STATE_TERMINATED:
			qthread_debug(THREAD_DETAILS,
				      "qthread_shepherd(%u): thread %i terminated\n",
				      me->shepherd_id, t->thread_id);
			t->thread_state = QTHREAD_STATE_DONE;
			/* we can remove the stack and the context... */
			qthread_thread_free(t);
			break;
		}
	    }
	}
    }

#ifdef QTHREAD_SHEPHERD_PROFILING
    qtimer_free(total);
    qtimer_free(idle);
#endif
    qthread_debug(ALL_DETAILS, "qthread_shepherd(%u): finished\n",
		  me->shepherd_id);
    pthread_exit(NULL);
    return NULL;
}				       /*}}} */

static qthread_shepherd_t *qthread_find_active_shepherd(qthread_shepherd_id_t
							* l, unsigned int *d)
{				       /*{{{ */
    qthread_shepherd_id_t target = 0;
    qthread_shepherd_t *sheps = qlib->shepherds;
    const qthread_shepherd_id_t nsheps = qlib->nshepherds;

    qthread_debug(ALL_FUNCTIONS,
		  "qthread_find_active_shepherd(%p): from %i sheps\n", l,
		  (int)nsheps);
    if (l == NULL) {
	/* if l==NULL, there's no locality info, so just find the least-busy active shepherd */
	saligned_t busyness = 0;
	int found = 0;

	for (size_t i = 0; i < nsheps; i++) {
	    if (QTHREAD_CASLOCK_READ_UI(sheps[i].active)) {
		ssize_t shep_busy_level = 
		    qthread_internal_atomic_read_s(&sheps[i].ready->advisory_queuelen,
			    &sheps[i].ready->advisory_queuelen_m);

		if (found == 0) {
		    found = 1;
		    qthread_debug(ALL_FUNCTIONS,
				  "qthread_find_active_shepherd(%p): shep %i is the least busy (%i) so far\n",
				  l, (int)i, shep_busy_level);
		    busyness = shep_busy_level;
		    target = i;
		} else if (shep_busy_level < busyness ||
			   (shep_busy_level == busyness &&
			    random() % 2 == 0)) {
		    qthread_debug(ALL_FUNCTIONS,
				  "qthread_find_active_shepherd(%p): shep %i is the least busy (%i) so far\n",
				  l, (int)i, shep_busy_level);
		    busyness = shep_busy_level;
		    target = i;
		}
	    }
	}
	assert(found);
	if (found == 0) {
	    qthread_debug(ALL_FUNCTIONS,
			  "qthread_find_active_shepherd(%p): DID NOT FIND ANY ACTIVE SHEPHERDS!!!\n",
			  l);
	    return NULL;
	} else {
	    qthread_debug(ALL_FUNCTIONS,
			  "qthread_find_active_shepherd(%p): found bored target %i\n",
			  l, (int)target);
	    return &(sheps[target]);
	}
    } else {
	/* if we have locality info, use it to identify the closest shepherd(s)
	 * and if there's more than one that is equidistant, pick the least busy
	 */
	qthread_shepherd_id_t alt;
	saligned_t busyness;

	while (target < nsheps && QTHREAD_CASLOCK_READ_UI(sheps[l[target]].active) == 0) {
	    target++;
	}
	if (target == nsheps) {
	    return NULL;
	}
	qthread_debug(ALL_FUNCTIONS,
		      "qthread_find_active_shepherd(%p): nearest active shepherd (%i) is %i away\n",
		      l, (int)l[target], (int)d[l[target]]);
	busyness =
	    qthread_internal_atomic_read_s(&sheps[l[target]].ready->advisory_queuelen,
		    &sheps[l[target]].ready->advisory_queuelen_m);
	for (alt = target + 1; alt < nsheps && d[l[alt]] == d[l[target]];
	     alt++) {
	    saligned_t shep_busy_level =
		qthread_internal_atomic_read_s(&sheps[l[alt]].ready->advisory_queuelen,
			&sheps[l[alt]].ready->advisory_queuelen_m);
	    if (shep_busy_level < busyness ||
		(shep_busy_level == busyness && random() % 2 == 0)) {
		qthread_debug(ALL_FUNCTIONS,
			      "qthread_find_active_shepherd(%p): shep %i is the least busy (%i) so far\n",
			      l, (int)d[l[alt]], shep_busy_level);
		busyness = shep_busy_level;
		target = alt;
	    }
	}
	qthread_debug(ALL_FUNCTIONS,
		      "qthread_find_active_shepherd(%p): found target %i\n",
		      l, (int)target);
	return &(sheps[l[target]]);
    }
}				       /*}}} */

int qthread_init(qthread_shepherd_id_t nshepherds)
{				       /*{{{ */
    char newenv[100] = {0};
    snprintf(newenv, 99, "QTHREAD_NUM_SHEPHERDS=%i", nshepherds);
    putenv(newenv);
    return qthread_initialize();
}				       /*}}} */

int qthread_initialize(void)
{				       /*{{{ */
    int r;
    size_t i;
    int need_sync = 1;
    qthread_shepherd_id_t nshepherds = 0;

#ifdef QTHREAD_HAVE_LGRP
    lgrp_cookie_t lgrp_cookie = lgrp_init(LGRP_VIEW_OS);
#endif

    qassert(QTHREAD_FASTLOCK_INIT(output_lock), 0);

#ifdef QTHREAD_DEBUG
    {
	char *qdl = getenv("QTHREAD_DEBUG_LEVEL");
	char *qdle = NULL;

	if (qdl) {
	    debuglevel = strtol(qdl, &qdle, 0);
	    if (qdle == NULL || qdle == qdl) {
		fprintf(stderr, "unparsable debug level (%s)\n", qdl);
		debuglevel = 0;
	    }
	} else {
	    debuglevel = 0;
	}
    }
#endif

    qthread_debug(ALL_CALLS, "qthread_init(): began.\n");
    if (qlib != NULL) {
	qthread_debug(ALL_DETAILS, "qthread_init(): redundant call\n");
	return QTHREAD_SUCCESS;
    }

#ifdef QTHREAD_USE_PTHREADS
    {
	char *qsh = getenv("QTHREAD_NUM_SHEPHERDS");
	char *qshe = NULL;

	if (qsh) {
	    nshepherds = strtol(qsh, &qshe, 0);
	    if (qshe == NULL || qshe == qsh) {
		fprintf(stderr, "unparsable number of shepherds (%s)\n", qsh);
		nshepherds = 0;
	    } else if (nshepherds > 0) {
		fprintf(stderr, "Forced %i Shepherds\n", nshepherds);
	    }
	} else {
	    nshepherds = 0;
	}
    }
    if (nshepherds == 0) {	       /* try to guess the "right" number */
#ifdef QTHREAD_HAVE_LIBNUMA
	if (numa_available() != -1) {
	    /* XXX: this logic is totally wrong for multithreaded shepherds */
# ifdef HAVE_NUMA_NUM_THREAD_CPUS
	    /* note: not numa_num_configured_cpus(), just in case an
	     * artificial limit has been imposed. */
	    nshepherds = numa_num_thread_cpus();
# elif defined(HAVE_NUMA_BITMASK_NBYTES)
	    for (size_t i=0;i<numa_bitmask_nbytes(numa_all_cpus_ptr)*8;i++) {
		nshepherds += numa_bitmask_isbitset(numa_all_cpus_ptr, i);
	    }
# else
	    /* this is (probably) optimal if/when we have multithreaded shepherds */
	    nshepherds = numa_max_node() + 1;
# endif
	}
#elif defined(QTHREAD_HAVE_LGRP)
	/* XXX: this is totally wrong for multithreaded shepherds */
	nshepherds =
	    lgrp_cpus(lgrp_cookie, lgrp_root(lgrp_cookie), NULL, 0,
		      LGRP_CONTENT_ALL);
#elif defined(HAVE_SYSCONF) && ! defined(QTHREAD_HAVE_MACHTOPO) && defined(_SC_NPROCESSORS_CONF) /* Linux */
	long ret = sysconf(_SC_NPROCESSORS_CONF);
	nshepherds = (ret > 0) ? ret : 1;
#elif defined(HAVE_SYSCTL) && defined(CTL_HW) && defined(HW_NCPU)
	int name[2] = { CTL_HW, HW_NCPU };
	uint32_t oldv;
	size_t oldvlen = sizeof(oldv);
	if (sysctl(name, 2, &oldv, &oldvlen, NULL, 0) < 0) {
# ifdef QTHREAD_HAVE_MACHTOPO
	    /* sysctl is the official query mechanism on Macs, so if it failed,
	     * we want to know */
	    perror("sysctl");
# endif
	} else {
	    assert(oldvlen == sizeof(oldv));
	    nshepherds = (int)oldv;
	}
#endif
	if (nshepherds <= 0) {
	    nshepherds = 1;
	}
    }

    if (nshepherds == 1) {
	need_sync = 0;
    }
#else
    nshepherds = 1;
    syncmode |= COLLECTION_MODE_NOSYNC;
    need_sync = 0;
#endif
    qthread_debug(THREAD_BEHAVIOR,"qthread_init(): there will be %u shepherd(s)\n", (unsigned)nshepherds);
    qlib = (qlib_t) malloc(sizeof(struct qlib_s));
    if (qlib == NULL) {
	return QTHREAD_MALLOC_ERROR;
    }

    /* initialize the FEB-like locking structures */

    /* this is synchronized with read/write locks by default */
    for (i = 0; i < QTHREAD_LOCKING_STRIPES; i++) {
#ifdef QTHREAD_COUNT_THREADS
	qlib->locks_stripes[i] = 0;
	qlib->febs_stripes[i] = 0;
# ifdef QTHREAD_MUTEX_INCREMENT
	QTHREAD_FASTLOCK_INIT(qlib->locks_stripes_locks[i]);
	QTHREAD_FASTLOCK_INIT(qlib->febs_stripes_locks[i]);
# endif
#endif
	if ((qlib->locks[i] = qt_hash_create(need_sync)) == NULL) {
	    return QTHREAD_MALLOC_ERROR;
	}
	if ((qlib->FEBs[i] = qt_hash_create(need_sync)) == NULL) {
	    return QTHREAD_MALLOC_ERROR;
	}
    }

    /* initialize the kernel threads and scheduler */
    qassert(pthread_key_create(&shepherd_structs, NULL), 0);
    qlib->nshepherds = nshepherds;
    if ((qlib->shepherds = (qthread_shepherd_t *)
	 calloc(nshepherds, sizeof(qthread_shepherd_t))) == NULL) {
	return QTHREAD_MALLOC_ERROR;
    }

    {
	char *stacksize = getenv("QTHREAD_STACK_SIZE");
	size_t ss = 0;

	if (stacksize && *stacksize) {
	    char *eptr;

	    ss = strtoul(stacksize, &eptr, 0);
	    if (*eptr != 0) {
		ss = 0;
	    }
	}
	if (ss != 0) {
	    qlib->qthread_stack_size = ss;
	} else {
	    qlib->qthread_stack_size = QTHREAD_DEFAULT_STACK_SIZE;
	}
	qthread_debug(THREAD_DETAILS, "qthread_init(): qthread stack size: %u\n", qlib->qthread_stack_size);
    }
#ifdef QTHREAD_GUARD_PAGES
    {
	size_t pagesize = getpagesize();

	/* round stack size to nearest page */
	if (qlib->qthread_stack_size % pagesize) {
	    qlib->qthread_stack_size +=
		pagesize - (qlib->qthread_stack_size % pagesize);
	}
    }
#endif
    qlib->max_thread_id = 0;
    qlib->sched_shepherd = 0;
    QTHREAD_FASTLOCK_INIT(qlib->max_thread_id_lock);
    QTHREAD_FASTLOCK_INIT(qlib->sched_shepherd_lock);
    {
	struct rlimit rlp;

	qassert(getrlimit(RLIMIT_STACK, &rlp), 0);
	qthread_debug(THREAD_DETAILS, "qthread_init(): stack sizes ... cur: %u max: %u\n",
		      rlp.rlim_cur, rlp.rlim_max);
	if (rlp.rlim_cur == RLIM_INFINITY) {
	    qlib->master_stack_size = 8 * 1024 * 1024;
	} else {
	    qlib->master_stack_size = (unsigned int)(rlp.rlim_cur);
	}
	qlib->max_stack_size = rlp.rlim_max;
    }

    /* initialize the shepherds as having no affinity */
    for (i = 0; i < nshepherds; i++) {
	qlib->shepherds[i].node = -1;
	qlib->shepherds[i].shep_dists = NULL;
	qlib->shepherds[i].sorted_sheplist = NULL;
    }
    {
	char *aff = getenv("QTHREAD_AFFINITY");

	if (aff && !strncmp(aff, "no", 3))
	    qaffinity = 0;
    }
    qthread_debug(ALL_DETAILS, "qthread_init(): qaffinity = %i\n", qaffinity);
    if (qaffinity == 1 && nshepherds > 1
#ifdef QTHREAD_HAVE_LIBNUMA
	&& numa_available() != -1
#endif
	) {			       /*{{{ */
#ifdef QTHREAD_HAVE_MACHTOPO
	/* there is no native way to detect distances, so unfortunately we must assume that they're all equidistant */
#elif defined(QTHREAD_HAVE_LIBNUMA)
	size_t max = numa_max_node() + 1;

	{
# ifdef QTHREAD_LIBNUMA_V2
	    struct bitmask *bmask = numa_allocate_nodemask();

	    assert(bmask);
	    numa_bitmask_clearall(bmask);
	    /* assign nodes */
	    for (i = 0; i < nshepherds; i++) {
		qlib->shepherds[i].node = i % max;
		numa_bitmask_setbit(bmask, i % max);
	    }
	    numa_set_interleave_mask(bmask);
	    numa_bitmask_free(bmask);
# else
	    nodemask_t bmask;

	    nodemask_zero(&bmask);
	    /* assign nodes */
	    for (i = 0; i < nshepherds; i++) {
		qlib->shepherds[i].node = i % max;
		nodemask_set(&bmask, i % max);
	    }
	    numa_set_interleave_mask(&bmask);
# endif
	}
	for (i = 0; i < nshepherds; i++) {
	    const unsigned int node_i = qlib->shepherds[i].node;
	    size_t j, k;
	    qlib->shepherds[i].shep_dists =
		calloc(nshepherds, sizeof(unsigned int));
	    assert(qlib->shepherds[i].shep_dists);
	    for (j = 0; j < nshepherds; j++) {
		const unsigned int node_j = qlib->shepherds[j].node;

		if (node_i != QTHREAD_NO_NODE && node_j != QTHREAD_NO_NODE) {
		    qlib->shepherds[i].shep_dists[j] =
			numa_distance(node_i, node_j);
		} else {
		    /* XXX too arbitrary */
		    if (i == j) {
			qlib->shepherds[i].shep_dists[j] = 0;
		    } else {
			qlib->shepherds[i].shep_dists[j] = 20;
		    }
		}
	    }
	    qlib->shepherds[i].sorted_sheplist =
		calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
	    assert(qlib->shepherds[i].sorted_sheplist);
	    k = 0;
	    for (j = 0; j < nshepherds; j++) {
		if (j != i) {
		    qlib->shepherds[i].sorted_sheplist[k++] = j;
		}
	    }
#  if defined(HAVE_QSORT_R) && QTHREAD_QSORT_BSD
	    assert(qlib->shepherds[i].sorted_sheplist);
	    qsort_r(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		    sizeof(qthread_shepherd_id_t), (void *)(intptr_t) i,
		    &qthread_internal_shepcomp);
#  elif defined(HAVE_QSORT_R) && QTHREAD_QSORT_GLIBC
	    /* what moron in the linux community decided to implement BSD's
	     * qsort_r with the arguments reversed??? */
	    assert(qlib->shepherds[i].sorted_sheplist);
	    qsort_r(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		    sizeof(qthread_shepherd_id_t),
		    &qthread_internal_shepcomp, (void *)(intptr_t) i);
#  else
	    shepcomp_src = (qthread_shepherd_id_t) i;
	    qsort(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		  sizeof(qthread_shepherd_id_t), qthread_internal_shepcomp);
#  endif
	}
#elif defined(QTHREAD_HAVE_LGRP)
	unsigned int lgrp_offset;
	int lgrp_count_grps;
	processorid_t **cpus = NULL;
	lgrp_id_t *lgrp_ids = NULL;

	switch (lgrp_cookie) {
	    case EINVAL:
	    case ENOMEM:
		qthread_debug(ALL_DETAILS, "lgrp_cookie is invalid!\n");
		return QTHREAD_THIRD_PARTY_ERROR;
	}
	{
	    size_t max_lgrps = lgrp_nlgrps(lgrp_cookie);

	    if (max_lgrps <= 0) {
		qthread_debug(ALL_DETAILS, "max_lgrps is <= zero! (%i)\n",
			      max_lgrps);
		return QTHREAD_THIRD_PARTY_ERROR;
	    }
	    cpus = calloc(max_lgrps, sizeof(processorid_t *));
	    assert(cpus);
	    lgrp_ids = calloc(max_lgrps, sizeof(lgrp_id_t));
	    assert(lgrp_ids);
	}
	lgrp_count_grps =
	    lgrp_walk(lgrp_cookie, lgrp_root(lgrp_cookie), cpus, lgrp_ids, 0);
	if (lgrp_count_grps <= 0) {
	    qthread_debug(ALL_DETAILS, "lgrp_count_grps is <= zero ! (%i)\n",
			  lgrp_count_grps);
	    return QTHREAD_THIRD_PARTY_ERROR;
	}
	for (i = 0; i < nshepherds; i++) {
	    /* first, pick a lgrp/node */
	    int cpu;
	    unsigned int first_loff;

	    first_loff = lgrp_offset = i % lgrp_count_grps;
	    qlib->shepherds[i].node = -1;
	    qlib->shepherds[i].lgrp = -1;
	    /* now pick an available CPU */
	    while (1) {
		cpu = 0;
		/* find an unused one */
		while (cpus[lgrp_offset][cpu] != (processorid_t) (-1))
		    cpu++;
		if (cpu == 0) {
		    /* if no unused ones... try the next lgrp */
		    lgrp_offset++;
		    lgrp_offset *= (lgrp_offset < lgrp_count_grps);
		    if (lgrp_offset == first_loff) {
			break;
		    }
		} else {
		    /* found one! */
		    cpu--;
		    qlib->shepherds[i].node = cpus[lgrp_offset][cpu];
		    qlib->shepherds[i].lgrp = lgrp_ids[lgrp_offset];
		    cpus[lgrp_offset][cpu] = -1;
		    break;
		}
	    }
	}
	for (i = 0; i < nshepherds; i++) {
	    const unsigned int node_i = qlib->shepherds[i].lgrp;
	    size_t j;
	    qlib->shepherds[i].shep_dists =
		calloc(nshepherds, sizeof(unsigned int));
	    assert(qlib->shepherds[i].shep_dists);
	    for (j = 0; j < nshepherds; j++) {
		const unsigned int node_j = qlib->shepherds[j].lgrp;

		if (node_i != QTHREAD_NO_NODE && node_j != QTHREAD_NO_NODE) {
		    int ret = lgrp_latency_cookie(lgrp_cookie, node_i, node_j,
						  LGRP_LAT_CPU_TO_MEM);

		    if (ret < 0) {
			assert(ret >= 0);
			return QTHREAD_THIRD_PARTY_ERROR;
		    } else {
			qlib->shepherds[i].shep_dists[j] = (unsigned int)ret;
		    }
		} else {
		    /* XXX too arbitrary */
		    if (i == j) {
			qlib->shepherds[i].shep_dists[j] = 12;
		    } else {
			qlib->shepherds[i].shep_dists[j] = 18;
		    }
		}
	    }
	}
	for (i = 0; i < nshepherds; i++) {
	    size_t j, k = 0;

	    qlib->shepherds[i].sorted_sheplist =
		calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
	    assert(qlib->shepherds[i].sorted_sheplist);
	    for (j = 0; j < nshepherds; j++) {
		if (j != i) {
		    qlib->shepherds[i].sorted_sheplist[k++] = j;
		}
	    }
#if defined(HAVE_QSORT_R) && !defined(__linux__)
	    qsort_r(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		    sizeof(qthread_shepherd_id_t), (void *)(intptr_t) i,
		    &qthread_internal_shepcomp);
#elif defined(HAVE_QSORT_R)
	    qsort_r(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		    sizeof(qthread_shepherd_id_t),
		    &qthread_internal_shepcomp, (void *)(intptr_t) i);
#else
	    shepcomp_src = (qthread_shepherd_id_t) i;
	    qsort(qlib->shepherds[i].sorted_sheplist, nshepherds - 1,
		  sizeof(qthread_shepherd_id_t), qthread_internal_shepcomp);
#endif
	}
	if (cpus) {
	    for (i = 0; i < lgrp_count_grps; i++) {
		free(cpus[i]);
	    }
	    free(cpus);
	}
	if (lgrp_ids) {
	    free(lgrp_ids);
	}
#else
# if defined(QTHREAD_USE_PLPA)
	/* there is no inherent way to detect distances, so unfortunately we must assume that they're all equidistant */
# endif
#endif
    }
    /*}}} */
#ifndef UNPOOLED
    /* set up the memory pools */
    for (i = 0; i < nshepherds; i++) { /*{{{ */
	/* the following SHOULD only be accessed by one thread at a time, so
	 * should be quite safe unsynchronized. If things fail, though...
	 * resynchronize them and see if that fixes it. */
	qlib->shepherds[i].qthread_pool =
	    qt_mpool_create(need_sync, sizeof(qthread_t),
			    qlib->shepherds[i].node);
	qlib->shepherds[i].stack_pool =
#ifdef QTHREAD_GUARD_PAGES
	    qt_mpool_create_aligned(need_sync,
				    qlib->qthread_stack_size +
				    (2 * getpagesize()),
				    qlib->shepherds[i].node, getpagesize());
#else
	    qt_mpool_create(need_sync, qlib->qthread_stack_size,
			    qlib->shepherds[i].node);
#endif
#if defined(ALIGNMENT_PROBLEMS_RETURN)
	if (sizeof(ucontext_t) < 2048) {
	    qlib->shepherds[i].context_pool =
		qt_mpool_create(need_sync, 2048, qlib->shepherds[i].node);
	} else {
	    qlib->shepherds[i].context_pool =
		qt_mpool_create(need_sync, sizeof(ucontext_t),
				qlib->shepherds[i].node);
	}
#else
	qlib->shepherds[i].context_pool =
	    qt_mpool_create(need_sync, sizeof(ucontext_t),
			    qlib->shepherds[i].node);
#endif
	qlib->shepherds[i].queue_pool =
	    qt_mpool_create(need_sync, sizeof(qthread_queue_t),
			    qlib->shepherds[i].node);
	qlib->shepherds[i].lfqueue_pool =
	    qt_mpool_create(need_sync, sizeof(qt_lfqueue_t),
			    qlib->shepherds[i].node);
	qlib->shepherds[i].lfqueue_node_pool =
	    qt_mpool_create_aligned(need_sync, sizeof(qt_lfqueue_node_t),
				    qlib->shepherds[i].node, 16);
	qlib->shepherds[i].lock_pool =
	    qt_mpool_create(need_sync, sizeof(qthread_lock_t),
			    qlib->shepherds[i].node);
	qlib->shepherds[i].addrres_pool =
	    qt_mpool_create(need_sync, sizeof(qthread_addrres_t),
			    qlib->shepherds[i].node);
	qlib->shepherds[i].addrstat_pool =
	    qt_mpool_create(need_sync, sizeof(qthread_addrstat_t),
			    qlib->shepherds[i].node);
    }				       /*}}} */
    /* these are used when qthread_fork() is called from a non-qthread. */
    generic_qthread_pool = qt_mpool_create(need_sync, sizeof(qthread_t), -1);
    generic_stack_pool =
#ifdef QTHREAD_GUARD_PAGES
	qt_mpool_create_aligned(need_sync,
				qlib->qthread_stack_size +
				(2 * getpagesize()), -1, getpagesize());
#else
	qt_mpool_create(need_sync, qlib->qthread_stack_size, -1);
#endif
    generic_context_pool = qt_mpool_create(need_sync, sizeof(ucontext_t), -1);
    generic_queue_pool =
	qt_mpool_create(need_sync, sizeof(qthread_queue_t), -1);
    generic_lfqueue_pool =
	qt_mpool_create(need_sync, sizeof(qt_lfqueue_t), -1);
    generic_lfqueue_node_pool =
	qt_mpool_create_aligned(need_sync, sizeof(qt_lfqueue_node_t), -1, 16);
    generic_lock_pool =
	qt_mpool_create(need_sync, sizeof(qthread_lock_t), -1);
    generic_addrstat_pool =
	qt_mpool_create(need_sync, sizeof(qthread_addrstat_t), -1);
#endif

    /* initialize the shepherd structures */
    for (i = 0; i < nshepherds; i++) {
	qthread_debug(ALL_DETAILS,
		      "qthread_init(): setting up shepherd %i (%p)\n", i,
		      &qlib->shepherds[i]);
	qlib->shepherds[i].shepherd_id = (qthread_shepherd_id_t) i;
	QTHREAD_CASLOCK_INIT(qlib->shepherds[i].active, 1);
	if ((qlib->shepherds[i].ready =
	     qt_lfqueue_new(&(qlib->shepherds[i]))) == NULL) {
	    perror("qthread_init creating shepherd queue");
	    return QTHREAD_MALLOC_ERROR;
	}
#ifdef QTHREAD_LOCK_PROFILING
	qlib->shepherds[i].uniquelockaddrs = qt_hash_create(0);
	qlib->shepherds[i].uniquefebaddrs = qt_hash_create(0);
#endif

	qthread_debug(ALL_DETAILS,
		      "qthread_init(): shepherd %i set up (%p)\n", i,
		      &qlib->shepherds[i]);

    }
	qthread_debug(ALL_DETAILS,
"qthread_init(): done setting up shepherds.\n");
    /* spawn the shepherds */
    for (i = 1; i < nshepherds; i++) {
	qthread_debug(ALL_DETAILS,
		      "qthread_init(): forking shepherd %i (%p)\n", i,
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


    /* now, transform the current main context into a qthread,
     * and make the main thread a shepherd (shepherd 0).
     * What will happen is this:
     * qlib->mccoy_thread represents the original execution thread, and so will
     * receive a context based on the current execution state.
     * qlib->master_context is a context for the new shepherd that will be
     * created (shepherd 0).
     * qlib->master_stack is a stack for that shepherd, and is huge, because
     * the shepherd expects a "standard" size stack. The mccoy_thread, as it is
     * for the *current* thread, also expects a full-size stack. The point of
     * this weirdness is so that the current thread can block the same way that
     * a qthread can. */
    qthread_debug(ALL_DETAILS, "allocating shep0\n");
    qlib->mccoy_thread = qthread_thread_new(NULL, NULL, NULL, 0);
    qthread_debug(ALL_DETAILS, "mccoy thread = %p\n", qlib->mccoy_thread);
    if (!qlib->mccoy_thread) {
	perror("qthread_init allocating qthread");
	return QTHREAD_MALLOC_ERROR;
    }

    if ((qlib->master_context = ALLOC_CONTEXT((&qlib->shepherds[0]))) == NULL) {
	perror("qthread_init allocating context for shepherd 0");
	return QTHREAD_MALLOC_ERROR;
    }
    qthread_debug(ALL_DETAILS, "master_context = %p\n", qlib->master_context);
    if ((qlib->master_stack = calloc(1, qlib->master_stack_size)) == NULL) {
	perror("qthread_init allocating stack for shepherd 0");
	return QTHREAD_MALLOC_ERROR;
    }
    qthread_debug(ALL_DETAILS, "master_stack = %p\n", qlib->master_stack);
#ifdef QTHREAD_USE_VALGRIND
    qlib->valgrind_masterstack_id =
	VALGRIND_STACK_REGISTER(qlib->master_stack, qlib->master_stack_size);
#endif

    /* the context will have its own stack ptr */
#ifdef QTHREAD_USE_VALGRIND
    VALGRIND_STACK_DEREGISTER(qlib->mccoy_thread->valgrind_stack_id);
#endif
    FREE_STACK(qlib->mccoy_thread->creator_ptr, qlib->mccoy_thread->stack);
    qlib->mccoy_thread->stack = NULL;
    qlib->mccoy_thread->thread_state = QTHREAD_STATE_YIELDED;	/* avoid re-launching */
    qlib->mccoy_thread->flags = QTHREAD_REAL_MCCOY;	/* i.e. this is THE parent thread */
    qlib->mccoy_thread->shepherd_ptr = &(qlib->shepherds[0]);

    qthread_debug(ALL_DETAILS, "qthread_init(): enqueueing mccoy thread\n");
    qt_lfqueue_enqueue(qlib->shepherds[0].ready, qlib->mccoy_thread,
		       &(qlib->shepherds[0]));
    qassert(getcontext(qlib->mccoy_thread->context), 0);
    qassert(getcontext(qlib->master_context), 0);
    /* now build the context for the shepherd 0 */
    qthread_debug(ALL_DETAILS, "qthread_init(): calling qthread_makecontext\n");
    qthread_makecontext(qlib->master_context, qlib->master_stack,
			qlib->master_stack_size,
#ifdef QTHREAD_MAKECONTEXT_SPLIT
			(void (*)(void))qthread_shepherd_wrapper,
#else
			(void (*)(void))qthread_shepherd,
#endif
			&(qlib->shepherds[0]), qlib->mccoy_thread->context);
#ifndef QTHREAD_NO_ASSERTS
    shep0arg = &(qlib->shepherds[0]);
#endif
    /* this launches shepherd 0 */
    qthread_debug(ALL_DETAILS, "qthread_init(): launching shepherd 0\n");
#ifdef QTHREAD_USE_VALGRIND
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(qlib->mccoy_thread->context,
				      sizeof(ucontext_t));
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(qlib->master_context,
				      sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(qlib->mccoy_thread->context,
			      sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(qlib->master_context, sizeof(ucontext_t));
#endif
    qthread_debug(ALL_DETAILS, "calling swapcontext\n");
    qassert(swapcontext(qlib->mccoy_thread->context, qlib->master_context),
	    0);

    qthread_debug(ALL_DETAILS, "calling atexit\n");
    atexit(qthread_finalize);

    qthread_debug(ALL_DETAILS, "qthread_init(): finished.\n");
    return QTHREAD_SUCCESS;
}				       /*}}} */

/* This initializes a context (c) to run the function (func) with a single
 * argument (arg). This is just a wrapper around makecontext that isolates some
 * of the portability garbage. */
static QINLINE void qthread_makecontext(ucontext_t * const c, void * const stack,
					const size_t stacksize, void (*func) (void),
					const void * const arg, ucontext_t * const returnc)
{				       /*{{{ */
#ifdef QTHREAD_MAKECONTEXT_SPLIT
    const unsigned int high = ((uintptr_t) arg) >> 32;
    const unsigned int low = ((uintptr_t) arg) & 0xffffffff;
#endif

    /* Several other libraries that do this reserve a few words on either end
     * of the stack for some reason. To avoid problems, I'll also do this (even
     * though I have no idea why they do this). */
#ifdef INVERSE_STACK_POINTER
    c->uc_stack.ss_sp = (char *)(stack) + stacksize - 8;
#else
    c->uc_stack.ss_sp = (char *)(stack) + 8;
#endif
    c->uc_stack.ss_size = stacksize - 64;
#ifdef UCSTACK_HAS_SSFLAGS
    c->uc_stack.ss_flags = 0;
#endif
#ifdef HAVE_NATIVE_MAKECONTEXT
    /* the makecontext man page (Linux) says: set the uc_link FIRST.
     * why? no idea */
    c->uc_link = returnc;	       /* NULL pthread_exit() */
#endif
#ifdef QTHREAD_MAKECONTEXT_SPLIT
# ifdef EXTRA_MAKECONTEXT_ARGC
    makecontext(c, func, 3, high, low);
# else
    makecontext(c, func, 2, high, low);
# endif /* EXTRA_MAKECONTEXT_ARGC */
#else /* QTHREAD_MAKECONTEXT_SPLIT */
# ifdef EXTRA_MAKECONTEXT_ARGC
    makecontext(c, func, 2, arg);
# else
    makecontext(c, func, 1, arg);
# endif /* EXTRA_MAKECONTEXT_ARGC */
#endif /* QTHREAD_MAKECONTEXT_SPLIT */
#ifdef HAVE_NATIVE_MAKECONTEXT
    assert((void*)c->uc_link == (void*)returnc);
#endif
}				       /*}}} */

void qthread_finalize(void)
{				       /*{{{ */
    int r;
    qthread_shepherd_id_t i;
    qthread_t *t;

    if (qlib == NULL)
	return;

#ifdef QTHREAD_LOCK_PROFILING
    double aquirelock_maxtime = 0.0;
    double aquirelock_time = 0.0;
    size_t aquirelock_count = 0;
    double lockwait_maxtime = 0.0;
    double lockwait_time = 0.0;
    size_t lockwait_count = 0;
    double hold_maxtime = 0.0;
    double hold_time = 0.0;
    double febblock_maxtime = 0.0;
    double febblock_time = 0.0;
    size_t febblock_count = 0;
    double febwait_maxtime = 0.0;
    double febwait_time = 0.0;
    size_t febwait_count = 0;
    double empty_maxtime = 0.0;
    double empty_time = 0.0;
    double empty_count = 0;
    qt_hash uniquelockaddrs = qt_hash_create(0);
    qt_hash uniquefebaddrs = qt_hash_create(0);
#endif

    qthread_debug(ALL_CALLS, "qthread_finalize(): began.\n");

    /* rcm - probably need to put a "turn off the library flag" here, but,
     * the programmer can ensure that no further threads are forked for now
     */

    /* enqueue the termination thread sentinal */
    for (i = 1; i < qlib->nshepherds; i++) {
	t = qthread_thread_bare(NULL, NULL, (aligned_t *) NULL, i);
	assert(t != NULL);	       /* what else can we do? */
	t->thread_state = QTHREAD_STATE_TERM_SHEP;
	t->thread_id = (unsigned int)-1;
	qt_lfqueue_enqueue(qlib->shepherds[i].ready, t,
			   &(qlib->shepherds[0]));
    }

#ifdef QTHREAD_SHEPHERD_PROFILING
#warning FIXME: total_time is always 0 on shepherd 0
    printf
	("QTHREADS: Shepherd 0 spent %f%% of the time idle (%f:%f) handling %lu threads\n",
	 qlib->shepherds[0].idle_time / qlib->shepherds[0].total_time * 100.0,
	 qlib->shepherds[0].total_time, qlib->shepherds[0].idle_time,
	 (unsigned long)qlib->shepherds[0].num_threads);
    printf
	("QTHREADS: Shepherd 0 averaged %g secs to find a new thread, max %g secs\n",
	 qlib->shepherds[0].idle_time / qlib->shepherds[0].idle_count,
	 qlib->shepherds[0].idle_maxtime);
#endif
    /* wait for each SPAWNED shepherd to drain it's queue
     * (note: not shepherd 0, because that one wasn't spawned) */
    for (i = 1; i < qlib->nshepherds; i++) {
	if ((r = pthread_join(qlib->shepherds[i].shepherd, NULL)) != 0) {
	    fprintf(stderr,
		    "qthread_finalize: pthread_join() of shep %i failed (%d, or \"%s\")\n",
		    i, r, strerror(r));
	    abort();
	}
	QTHREAD_CASLOCK_DESTROY(qlib->shepherds[i].active);
	qt_lfqueue_free(qlib->shepherds[i].ready);
#ifdef QTHREAD_SHEPHERD_PROFILING
	printf
	    ("QTHREADS: Shepherd %i spent %f%% of the time idle (%f:%f) handling %lu threads\n",
	     i,
	     qlib->shepherds[i].idle_time / qlib->shepherds[i].total_time *
	     100.0, qlib->shepherds[i].total_time,
	     qlib->shepherds[i].idle_time,
	     (unsigned long)qlib->shepherds[i].num_threads);
	printf
	    ("QTHREADS: Shepherd %i averaged %g secs to find a new thread, max %g secs\n",
	     i, qlib->shepherds[i].idle_time / qlib->shepherds[i].idle_count,
	     qlib->shepherds[i].idle_maxtime);
#endif
#ifdef QTHREAD_LOCK_PROFILING
	QTHREAD_ACCUM_MAX(aquirelock_maxtime,
			  qlib->shepherds[i].aquirelock_maxtime);
	aquirelock_time += qlib->shepherds[i].aquirelock_time;
	aquirelock_count += qlib->shepherds[i].aquirelock_count;
	QTHREAD_ACCUM_MAX(lockwait_maxtime,
			  qlib->shepherds[i].lockwait_maxtime);
	lockwait_time += qlib->shepherds[i].lockwait_time;
	lockwait_count += qlib->shepherds[i].lockwait_count;
	QTHREAD_ACCUM_MAX(hold_maxtime, qlib->shepherds[i].hold_maxtime);
	hold_time += qlib->shepherds[i].hold_time;
	QTHREAD_ACCUM_MAX(febblock_maxtime,
			  qlib->shepherds[i].febblock_maxtime);
	febblock_time += qlib->shepherds[i].febblock_time;
	febblock_count += qlib->shepherds[i].febblock_count;
	QTHREAD_ACCUM_MAX(febwait_maxtime,
			  qlib->shepherds[i].febwait_maxtime);
	febwait_time += qlib->shepherds[i].febwait_time;
	febwait_count += qlib->shepherds[i].febwait_count;
	QTHREAD_ACCUM_MAX(empty_maxtime, qlib->shepherds[i].empty_maxtime);
	empty_time += qlib->shepherds[i].empty_time;
	empty_count += qlib->shepherds[i].empty_count;
	qt_hash_callback(qlib->shepherds[i].uniquelockaddrs,
			 qthread_unique_collect, uniquelockaddrs);
	qt_hash_callback(qlib->shepherds[i].uniquefebaddrs,
			 qthread_unique_collect, uniquefebaddrs);
	qt_hash_destroy(qlib->shepherds[i].uniquelockaddrs);
	qt_hash_destroy(qlib->shepherds[i].uniquefebaddrs);
#endif
    }
    qt_lfqueue_free(qlib->shepherds[0].ready);

#ifdef QTHREAD_LOCK_PROFILING
    printf
	("QTHREADS: %llu locks aquired (%ld unique), average %g secs, max %g secs\n",
	 (unsigned long long)aquirelock_count, qt_hash_count(uniquelockaddrs),
	 (aquirelock_count == 0) ? 0 : (aquirelock_time / aquirelock_count),
	 aquirelock_maxtime);
    printf
	("QTHREADS: Blocked on a lock %llu times, average %g secs, max %g secs\n",
	 (unsigned long long)lockwait_count,
	 (lockwait_count == 0) ? 0 : (lockwait_time / lockwait_count),
	 lockwait_maxtime);
    printf("QTHREADS: Locks held an average of %g seconds, max %g seconds\n",
	   (aquirelock_count == 0) ? 0 : (hold_time / aquirelock_count),
	   hold_maxtime);
    printf("QTHREADS: %ld unique addresses used with FEB, blocked %g secs\n",
	   qt_hash_count(uniquefebaddrs),
	   (febblock_count == 0) ? 0 : febblock_time);
    printf
	("QTHREADS: %llu potentially-blocking FEB operations, average %g secs, max %g secs\n",
	 (unsigned long long)febblock_count,
	 (febblock_count == 0) ? 0 : (febblock_time / febblock_count),
	 febblock_maxtime);
    printf
	("QTHREADS: %llu FEB operations blocked, average wait %g secs, max %g secs\n",
	 (unsigned long long)febwait_count,
	 (febwait_count == 0) ? 0 : (febwait_time / febwait_count),
	 febwait_maxtime);
    printf
	("QTHREADS: %llu FEB bits emptied, stayed empty average %g secs, max %g secs\n",
	 (unsigned long long)empty_count,
	 (empty_count == 0) ? 0 : (empty_time / empty_count), empty_maxtime);
#endif

    for (i = 0; i < QTHREAD_LOCKING_STRIPES; i++) {
	qt_hash_destroy(qlib->locks[i]);
	qt_hash_destroy_deallocate(qlib->FEBs[i],
				   (qt_hash_deallocator_fn)
				   qthread_addrstat_delete);
#ifdef QTHREAD_COUNT_THREADS
	printf("QTHREADS: bin %i used %i/%i times\n", i,
	       qlib->locks_stripes[i], qlib->febs_stripes[i]);
# ifdef QTHREAD_MUTEX_INCREMENT
	QTHREAD_FASTLOCK_DESTROY(qlib->locks_stripes_locks[i]);
	QTHREAD_FASTLOCK_DESTROY(qlib->febs_stripes_locks[i]);
# endif
#endif
    }

#ifdef QTHREAD_COUNT_THREADS
    printf("spawned %lu threads, max concurrency %lu\n",
	   (unsigned long)threadcount, (unsigned long)maxconcurrentthreads);
    QTHREAD_DESTROYLOCK(&threadcount_lock);
    QTHREAD_DESTROYLOCK(&concurrentthreads_lock);
#endif

    QTHREAD_FASTLOCK_DESTROY(qlib->max_thread_id_lock);
    QTHREAD_FASTLOCK_DESTROY(qlib->sched_shepherd_lock);

    FREE_CONTEXT((&qlib->shepherds[0]), qlib->master_context);
    FREE_CONTEXT((&qlib->shepherds[0]), qlib->mccoy_thread->context);
#ifdef QTHREAD_USE_VALGRIND
    VALGRIND_STACK_DEREGISTER(qlib->mccoy_thread->valgrind_stack_id);
    VALGRIND_STACK_DEREGISTER(qlib->valgrind_masterstack_id);
#endif
    free(qlib->mccoy_thread->stack);
    free(qlib->master_stack);
    for (i = 0; i < qlib->nshepherds; ++i) {
	if (qlib->shepherds[i].shep_dists) {
	    free(qlib->shepherds[i].shep_dists);
	}
	if (qlib->shepherds[i].sorted_sheplist) {
	    free(qlib->shepherds[i].sorted_sheplist);
	}
    }
#ifndef UNPOOLED
    for (i = 0; i < qlib->nshepherds; ++i) {
	qt_mpool_destroy(qlib->shepherds[i].qthread_pool);
	qt_mpool_destroy(qlib->shepherds[i].queue_pool);
	qt_mpool_destroy(qlib->shepherds[i].lfqueue_pool);
	qt_mpool_destroy(qlib->shepherds[i].lfqueue_node_pool);
	qt_mpool_destroy(qlib->shepherds[i].lock_pool);
	qt_mpool_destroy(qlib->shepherds[i].addrres_pool);
	qt_mpool_destroy(qlib->shepherds[i].addrstat_pool);
	qt_mpool_destroy(qlib->shepherds[i].stack_pool);
	qt_mpool_destroy(qlib->shepherds[i].context_pool);
    }
    qt_mpool_destroy(generic_qthread_pool);
    qt_mpool_destroy(generic_stack_pool);
    qt_mpool_destroy(generic_context_pool);
    qt_mpool_destroy(generic_queue_pool);
    qt_mpool_destroy(generic_lfqueue_pool);
    qt_mpool_destroy(generic_lfqueue_node_pool);
    qt_mpool_destroy(generic_lock_pool);
    qt_mpool_destroy(generic_addrstat_pool);
#endif
    free(qlib->shepherds);
    free(qlib);
    qlib = NULL;
    qassert(pthread_key_delete(shepherd_structs), 0);

    qthread_debug(ALL_DETAILS, "qthread_finalize(): finished.\n");
}				       /*}}} */

int qthread_disable_shepherd(const qthread_shepherd_id_t shep)
{
    assert(shep < qlib->nshepherds);
    if (shep == 0) {
	/* currently, the "real mccoy" original thread cannot be migrated
	 * (because I don't know what issues that could cause on all
	 * architectures). For similar reasons, therefore, the original
	 * shepherd cannot be disabled. One of the nice aspects of this is that
	 * therefore it is impossible to disable ALL shepherds.
	 *
	 * ... it's entirely possible that I'm being overly cautious. This is a
	 * policy based on gut feeling rather than specific issues. */
	return QTHREAD_NOT_ALLOWED;
    }
    qthread_debug(ALL_CALLS, "qthread_disable_shepherd(%i)\n", shep);
    QT_CAS(qlib->shepherds[shep].active, 1, 0);
    return QTHREAD_SUCCESS;
}

void qthread_enable_shepherd(const qthread_shepherd_id_t shep)
{
    assert(shep < qlib->nshepherds);
    qthread_debug(ALL_CALLS, "qthread_enable_shepherd(%i)\n", shep);
    QT_CAS(qlib->shepherds[shep].active, 0, 1);
}

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
    const qthread_t * f = t;
    if (t != NULL && t->stack != NULL) {
	assert((size_t) & f > (size_t) t->stack &&
	       (size_t) & f < ((size_t) t->stack + qlib->qthread_stack_size));
#ifdef STACK_GROWS_DOWN
	/* not tested */
	assert(((size_t) (t->stack) + qlib->qthread_stack_size) -
	       (size_t) (&f) < qlib->qthread_stack_size);
	return ((size_t) (t->stack) + qlib->qthread_stack_size) -
	    (size_t) (&f);
#else
	assert((size_t) (&f) - (size_t) (t->stack) <
	       qlib->qthread_stack_size);
	return (size_t) (&f) - (size_t) (t->stack);
#endif
    } else {
	return 0;
    }
}				       /*}}} */

aligned_t *qthread_retloc(const qthread_t * t)
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

static QINLINE qthread_t *qthread_thread_bare(const qthread_f f,
					      const void *arg,
					      aligned_t * ret,
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
				  &qlib->max_thread_id_lock, 1);
#else
	t->thread_id = (unsigned int)-1;
#endif
	t->flags = 0;
	t->thread_state = QTHREAD_STATE_NEW;
	t->f = f;
	t->arg = (void *)arg;
	t->blockedon = NULL;
	t->shepherd_ptr = &(qlib->shepherds[shepherd]);
	t->target_shepherd = NULL;
	t->ret = ret;
	t->context = NULL;
	t->stack = NULL;
    }
    return t;
}				       /*}}} */

static QINLINE int qthread_thread_plush(qthread_t * t)
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
#ifdef QTHREAD_USE_VALGRIND
	    t->valgrind_stack_id =
		VALGRIND_STACK_REGISTER(stack, qlib->qthread_stack_size);
#endif
	    return QTHREAD_SUCCESS;
	}
	FREE_CONTEXT(shepherd, uc);
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

/* this could be reduced to a qthread_thread_bare() and qthread_thread_plush(),
 * but I *think* doing it this way makes it faster. maybe not, I haven't tested
 * it. */
static QINLINE qthread_t *qthread_thread_new(const qthread_f f,
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

    qthread_debug(ALL_FUNCTIONS, "qthread_thread_new(): myshep=%p\n", myshep);

    t = ALLOC_QTHREAD(myshep);
    qthread_debug(ALL_DETAILS, "qthread_thread_new(): t = %p\n", t);
    if (t == NULL) {
	return NULL;
    }
    uc = ALLOC_CONTEXT(myshep);
    qthread_debug(ALL_DETAILS, "qthread_thread_new(): uc = %p\n", uc);
    if (uc == NULL) {
	FREE_QTHREAD(t);
	return NULL;
    }
    stack = ALLOC_STACK(myshep);
    qthread_debug(ALL_DETAILS, "qthread_thread_new(): stack = %p\n", stack);
    if (stack == NULL) {
	FREE_QTHREAD(t);
	FREE_CONTEXT(myshep, uc);
	return NULL;
    }
#ifdef QTHREAD_USE_VALGRIND
    t->valgrind_stack_id =
	VALGRIND_STACK_REGISTER(stack, qlib->qthread_stack_size);
#endif

    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = (void *)arg;
    t->blockedon = NULL;
    t->shepherd_ptr = &(qlib->shepherds[shepherd]);
    t->target_shepherd = NULL;
    t->ret = ret;
    t->flags = 0;
    t->context = uc;
    t->stack = stack;

#ifdef QTHREAD_NONLAZY_THREADIDS
    /* give the thread an ID number */
    t->thread_id =
	qthread_internal_incr(&(qlib->max_thread_id),
			      &qlib->max_thread_id_lock, 1);
#else
    t->thread_id = (unsigned int)-1;
#endif

    qthread_debug(ALL_DETAILS, "qthread_thread_new(): returning\n");
    return t;
}				       /*}}} */

static QINLINE void qthread_thread_free(qthread_t * t)
{				       /*{{{ */
    assert(t != NULL);

    if (t->context) {
	FREE_CONTEXT(t->creator_ptr, t->context);
    }
    if (t->stack != NULL) {
#ifdef QTHREAD_USE_VALGRIND
	VALGRIND_STACK_DEREGISTER(t->valgrind_stack_id);
#endif
	FREE_STACK(t->creator_ptr, t->stack);
    }
    FREE_QTHREAD(t);
}				       /*}}} */


/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

// This lock-free algorithm borrowed from
// http://www.research.ibm.com/people/m/michael/podc-1996.pdf

/* to avoid ABA reinsertion trouble, each pointer in the queue needs to have a
 * monotonically increasing counter associated with it. The counter doesn't
 * need to be huge, just big enough to avoid trouble. We'll
 * just claim 4, to be conservative. Thus, a qt_lfqueue_node_t must be at least
 * 16 bytes. */
#if defined(QTHREAD_USE_VALGRIND) && NO_ABA_PROTECTION
# define QPTR(x) (x)
# define QCTR(x) 0
# define QCOMPOSE(x,y) (x)
#else
# define QCTR_MASK (15)
# define QPTR(x) ((volatile qt_lfqueue_node_t*)(((uintptr_t)(x))&~(uintptr_t)QCTR_MASK))
# define QCTR(x) (((uintptr_t)(x))&QCTR_MASK)
# define QCOMPOSE(x,y) (void*)(((uintptr_t)QPTR(x))|((QCTR(y)+1)&QCTR_MASK))
#endif

static QINLINE qt_lfqueue_t *qt_lfqueue_new(qthread_shepherd_t * shepherd)
{				       /*{{{ */
    qt_lfqueue_t *q = ALLOC_LFQUEUE(shepherd);

    if (q != NULL) {
	q->creator_ptr = shepherd;
#ifdef QTHREAD_MUTEX_INCREMENT
	if (QTHREAD_FASTLOCK_INIT(q->head_lock) != 0) {
	    FREE_LFQUEUE(q);
	    return NULL;
	}
	if (QTHREAD_FASTLOCK_INIT(q->tail_lock) != 0) {
	    qassert(QTHREAD_FASTLOCK_DESTROY(q->head_lock), 0);
	    FREE_LFQUEUE(q);
	}
	if (QTHREAD_FASTLOCK_INIT(q->advisory_queuelen_m) != 0) {
	    qassert(QTHREAD_FASTLOCK_DESTROY(q->head_lock), 0);
	    qassert(QTHREAD_FASTLOCK_DESTROY(q->tail_lock), 0);
	    FREE_LFQUEUE(q);
	}
	ALLOC_LFQNODE(((qt_lfqueue_node_t **) & (q->head)), shepherd);
	assert(q->head != NULL);
	if (q->head == NULL) {
	    QTHREAD_FASTLOCK_DESTROY(q->advisory_queuelen_m);
	    QTHREAD_FASTLOCK_DESTROY(q->head_lock);
	    QTHREAD_FASTLOCK_DESTROY(q->tail_lock);
	    FREE_LFQUEUE(q);
	    q = NULL;
	} else {
	    q->tail = q->head;
	    q->head->next = NULL;
	}
#else
# ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
	if (pthread_mutex_init(&q->lock, NULL) != 0) {
	    FREE_LFQUEUE(q);
	    return NULL;
	}
	if (pthread_cond_init(&q->notempty, NULL) != 0) {
	    QTHREAD_DESTROYLOCK(&q->lock);
	    FREE_LFQUEUE(q);
	    return NULL;
	}
	q->fruitless = 0;
# endif
	ALLOC_LFQNODE(((qt_lfqueue_node_t **) & (q->head)), shepherd);
	assert(QPTR(q->head) != NULL);
	if (QPTR(q->head) == NULL) {   // if we're not using asserts, fail nicely
# ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
	    QTHREAD_DESTROYLOCK(&q->lock);
	    QTHREAD_DESTROYCOND(&q->notempty);
# endif
	    FREE_LFQUEUE(q);
	    q = NULL;
	}
	q->tail = q->head;
	QPTR(q->tail)->next = NULL;
#endif
    }
    return q;
}				       /*}}} */

static QINLINE void qt_lfqueue_free(qt_lfqueue_t * q)
{				       /*{{{ */
#ifdef QTHREAD_MUTEX_INCREMENT
    while (q->head != q->tail) {
	qt_lfqueue_dequeue(q);
    }
    QTHREAD_FASTLOCK_DESTROY(q->head_lock);
    QTHREAD_FASTLOCK_DESTROY(q->tail_lock);
    QTHREAD_FASTLOCK_DESTROY(q->advisory_queuelen_m);
#else
    while (QPTR(q->head) != QPTR(q->tail)) {
	qt_lfqueue_dequeue(q);
    }
    assert(QPTR(q->head) == QPTR(q->tail));
# ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
    QTHREAD_DESTROYLOCK(&q->lock);
    QTHREAD_DESTROYCOND(&q->notempty);
# endif
#endif /* MUTEX queue */
    FREE_LFQNODE((qt_lfqueue_node_t *) QPTR(q->head));
    FREE_LFQUEUE(q);
}				       /*}}} */

static QINLINE void qt_lfqueue_enqueue(qt_lfqueue_t * q, qthread_t * t,
				       qthread_shepherd_t * shep)
{				       /*{{{ */
#ifdef QTHREAD_MUTEX_INCREMENT
    qt_lfqueue_node_t *node;

    ALLOC_LFQNODE(&node, shep);
    assert(node != NULL);
    node->value = t;
    node->next = NULL;
    QTHREAD_FASTLOCK_LOCK(&q->tail_lock);
    {
	q->tail->next = node;
	q->tail = node;
    }
    QTHREAD_FASTLOCK_UNLOCK(&q->tail_lock);
#else
    volatile qt_lfqueue_node_t *tail;
    volatile qt_lfqueue_node_t *next;
    qt_lfqueue_node_t *node;

    assert(t != NULL);
    assert(q != NULL);

    ALLOC_LFQNODE(&node, shep);
    assert(node != NULL);
    assert(QCTR(node) == 0);	       // node MUST be aligned

    node->value = t;
    // set to null without disturbing the ctr
    node->next = (qt_lfqueue_node_t *) (uintptr_t) QCTR(node->next);

    while (1) {
	tail = _(q->tail);
	next = _(QPTR(tail)->next);
	if (tail == _(q->tail)) {      // are tail and next consistent?
	    if (QPTR(next) == NULL) {  // was tail pointing to the last node?
		if (qt_cas
		    ((void *volatile *)&(QPTR(tail)->next), (void *)next,
		     QCOMPOSE(node, next)) == next)
		    break;	       // success!
	    } else {		       // tail not pointing to last node
		(void)qt_cas((void *volatile *)&(q->tail), (void *)tail,
			     QCOMPOSE(next, tail));
	    }
	}
    }
    (void)qt_cas((void *volatile *)&(q->tail), (void *)tail,
		 QCOMPOSE(node, tail));
    qthread_incr(&q->advisory_queuelen, 1);
# ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
    if (vol_read_a(&(q->fruitless))) {
	QTHREAD_LOCK(&q->lock);
	if (vol_read_a(&(q->fruitless))) {
	    *vol_id_a(&(q->fruitless)) = 0;
	    QTHREAD_SIGNAL(&q->notempty);
	}
	QTHREAD_UNLOCK(&q->lock);
    }
# endif
#endif
}				       /*}}} */

static QINLINE qthread_t *qt_lfqueue_dequeue(qt_lfqueue_t * q)
{				       /*{{{ */
    qthread_t *p = NULL;
#ifdef QTHREAD_MUTEX_INCREMENT
    qt_lfqueue_node_t *node, *new_head;

    assert(q != NULL);
    QTHREAD_FASTLOCK_LOCK(&q->head_lock);
    {
	node = q->head;
	new_head = node->next;
	if (new_head != NULL) {
	    p = new_head->value;
	    q->head = new_head;
	}
    }
    QTHREAD_FASTLOCK_UNLOCK(&q->head_lock);
#else
    volatile qt_lfqueue_node_t *head;
    volatile qt_lfqueue_node_t *tail;
    volatile qt_lfqueue_node_t *next_ptr;

    assert(q != NULL);
    while (1) {
	head = _(q->head);
	tail = _(q->tail);
	next_ptr = QPTR(_(QPTR(head)->next));
	if (head == _(q->head)) {      // are head, tail, and next consistent?
	    if (QPTR(head) == QPTR(tail)) {	// is queue empty or tail falling behind?
		if (next_ptr == NULL) {	// is queue empty?
		    return NULL;
		}
		(void)qt_cas((void *volatile *)&(q->tail), (void *)tail, QCOMPOSE(next_ptr, tail));	// advance tail ptr
	    } else {		       // no need to deal with tail
		// read value before CAS, otherwise another dequeue might free the next node
		p = next_ptr->value;
		if (qt_cas
		    ((void *volatile *)&(q->head), (void *)head,
		     QCOMPOSE(next_ptr, head)) == head) {
		    break;	       // success!
		}
	    }
	}
    }
    FREE_LFQNODE((qt_lfqueue_node_t *) QPTR(head));
#endif
    if (p != NULL) {
	qthread_internal_incr_s(&q->advisory_queuelen, &q->advisory_queuelen_m, -1);
    }
    return p;
}				       /*}}} */

/* this function is amusing, but the point is to avoid unnecessary bus traffic
 * by allowing idle shepherds to sit for a while while still allowing for
 * low-overhead for busy shepherds. This is a hybrid approach: normally, it
 * functions as a spinlock, but if it spins too much, it waits for a signal */
static QINLINE qthread_t *qt_lfqueue_dequeue_blocking(qt_lfqueue_t * q)
{				       /*{{{ */
    qthread_t *p = NULL;
#ifdef QTHREAD_MUTEX_INCREMENT
    while ((p = qt_lfqueue_dequeue(q)) == NULL) {
    }
#else
    volatile qt_lfqueue_node_t *head;
    volatile qt_lfqueue_node_t *tail;
    volatile qt_lfqueue_node_t *next_ptr;

    assert(q != NULL);
  lfqueue_dequeue_restart:
    while (1) {
	head = _(q->head);
	tail = _(q->tail);
	next_ptr = QPTR(_(QPTR(head)->next));
	if (head == _(q->head)) {      // are head, tail, and next consistent?
	    if (QPTR(head) == QPTR(tail)) {	// is queue empty or tail falling behind?
		if (next_ptr == NULL) {	// is queue empty?
#ifdef QTHREAD_CONDWAIT_BLOCKING_QUEUE
		    if (qthread_internal_incr(&q->fruitless, &q->fruitless_m, 1) > 1000) {
			QTHREAD_LOCK(&q->lock);
			while (vol_read_a(&q->fruitless) > 1000) {
			    QTHREAD_CONDWAIT(&q->notempty, &q->lock);
			}
			QTHREAD_UNLOCK(&q->lock);
		    } else {
#ifdef HAVE_PTHREAD_YIELD
			pthread_yield();
#elif HAVE_SHED_YIELD
			sched_yield();
#endif
		    }
#endif
		    goto lfqueue_dequeue_restart;
		}
		(void)qt_cas((void *volatile *)&(q->tail), (void *)tail, QCOMPOSE(next_ptr, tail));	// advance tail ptr
	    } else {		       // no need to deal with tail
		// read value before CAS, otherwise another dequeue might free the next node
		p = next_ptr->value;
		if (qt_cas
		    ((void *volatile *)&(q->head), (void *)head,
		     QCOMPOSE(next_ptr, head)) == head) {
		    break;	       // success!
		}
	    }
	}
    }
    FREE_LFQNODE((qt_lfqueue_node_t *) QPTR(head));
    if (p != NULL) {
	qthread_internal_incr_s(&q->advisory_queuelen, &q->advisory_queuelen_m, -1);
    }
#endif
    return p;
}				       /*}}} */

static QINLINE qthread_queue_t *qthread_queue_new(qthread_shepherd_t *
						  shepherd)
{				       /*{{{ */
    qthread_queue_t *q;

    q = ALLOC_QUEUE(shepherd);
    if (q != NULL) {
	q->head = NULL;
	q->tail = NULL;
	if (pthread_mutex_init(&q->lock, NULL) != 0) {
	    FREE_QUEUE(q);
	    return NULL;
	}
	if (pthread_cond_init(&q->notempty, NULL) != 0) {
	    QTHREAD_DESTROYLOCK(&q->lock);
	    FREE_QUEUE(q);
	    return NULL;
	}
    }
    return q;
}				       /*}}} */

static QINLINE void qthread_queue_free(qthread_queue_t * q)
{				       /*{{{ */
    assert((q->head == NULL) && (q->tail == NULL));
    QTHREAD_DESTROYLOCK(&q->lock);
    QTHREAD_DESTROYCOND(&q->notempty);
    FREE_QUEUE(q);
}				       /*}}} */

static QINLINE void qthread_enqueue(qthread_queue_t * q, qthread_t * t)
{				       /*{{{ */
    assert(t != NULL);
    assert(q != NULL);

    qthread_debug(ALL_FUNCTIONS, "qthread_enqueue(%p,%p): started\n", q, t);

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

    qthread_debug(ALL_DETAILS, "qthread_enqueue(%p,%p): finished\n", q, t);
    QTHREAD_UNLOCK(&q->lock);
}				       /*}}} */

#if 0				       /* unused */
static QINLINE qthread_t *qthread_dequeue(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t;

    qthread_debug(ALL_FUNCTIONS, "qthread_dequeue(%p): started\n", q);

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

    qthread_debug(ALL_DETAILS, "qthread_dequeue(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */
#endif

static QINLINE qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t = NULL;

    /* NOTE: it's up to the caller to lock/unlock the queue! */
    qthread_debug(ALL_FUNCTIONS,
		  "qthread_dequeue_nonblocking(%p,%p): started\n", q, t);

    if (q->head == NULL) {
	qthread_debug(ALL_DETAILS,
		      "qthread_dequeue_nonblocking(%p,%p): finished (nobody in list)\n",
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

    qthread_debug(ALL_DETAILS,
		  "qthread_dequeue_nonblocking(%p,%p): finished\n", q, t);
    return (t);
}				       /*}}} */

/* this function is for maintenance of the FEB hashtables. SHOULD only be
 * necessary for things left over when qthread_finalize is called */
static void qthread_addrstat_delete(qthread_addrstat_t * m)
{				       /*{{{ */
#ifdef QTHREAD_LOCK_PROFILING
    qtimer_free(m->empty_timer);
#endif
    QTHREAD_DESTROYLOCK(&m->lock);
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

    qthread_debug(THREAD_BEHAVIOR,
		  "qthread_wrapper(): tid %u executing f=%p arg=%p...\n",
		  t->thread_id, t->f, t->arg);
    assert((size_t)&t > (size_t)t->stack &&
	   (size_t)&t < ((size_t)t->stack + qlib->qthread_stack_size));
#ifdef QTHREAD_COUNT_THREADS
    qthread_internal_incr(&threadcount, &threadcount_lock, 1);
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

#ifdef QTHREAD_COUNT_THREADS
    qassert(pthread_mutex_lock(&concurrentthreads_lock), 0);
    concurrentthreads--;
    qassert(pthread_mutex_unlock(&concurrentthreads_lock), 0);
#endif
    if (t->flags & QTHREAD_FUTURE) {
	future_exit(t);
    }
    /* theoretically, we could rely on the uc_link pointer to bring us back to
     * the parent shepherd. HOWEVER, this doesn't work in lots of situations,
     * so we do it manually. A brief list of situations:
     *  1. if we're using the portable make/get/swapcontext
     *  2. if the context switch requires a stack-size modification
     *  3. if the thread has migrated (i.e. uc_link points to the original
     *  shepherd, not the current parent... theoretically, that could be
     *  changed, but getting a good uc_link is finicky)
     *
     * Thus, since doing it manually isn't a performance problem, we do it
     * manually.
     */
    qthread_debug(THREAD_BEHAVIOR, "qthread_wrapper(): tid %u exiting.\n",
		  t->thread_id);
    qthread_back_to_master(t);
}				       /*}}} */

/* This function means "run thread t". The second argument (c) is a pointer
 * to the current context. */
static QINLINE void qthread_exec(qthread_t * t, ucontext_t * c)
{				       /*{{{ */
#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    assert(t != NULL);
    assert(c != NULL);

    if (t->thread_state == QTHREAD_STATE_NEW) {

	qthread_debug(ALL_DETAILS,
		      "qthread_exec(%p, %p): type is QTHREAD_THREAD_NEW!\n",
		      t, c);
	t->thread_state = QTHREAD_STATE_RUNNING;

	qassert(getcontext(t->context), 0);	/* puts the current context into t->context */
	qthread_makecontext(t->context, t->stack, qlib->qthread_stack_size,
			    (void (*)(void))qthread_wrapper, t, c);
#ifdef HAVE_NATIVE_MAKECONTEXT
    } else {
	t->context->uc_link = c;       /* NULL pthread_exit() */
#endif
    }

    t->return_context = c;

#ifdef NEED_RLIMIT
    qthread_debug(ALL_DETAILS,
		  "qthread_exec(%p): setting stack size limits... hopefully we don't currently exceed them!\n",
		  t);
    if (t->flags & QTHREAD_REAL_MCCOY) {
	rlp.rlim_cur = qlib->master_stack_size;
    } else {
	rlp.rlim_cur = qlib->qthread_stack_size;
    }
    rlp.rlim_max = qlib->max_stack_size;
    qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
#endif

    qthread_debug(ALL_DETAILS,
		  "qthread_exec(%p): executing swapcontext()...\n", t);
    /* return_context (aka "c") is being written over with the current context */
#ifdef QTHREAD_USE_VALGRIND
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(t->context, sizeof(ucontext_t));
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(t->return_context, sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(t->context, sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(t->return_context, sizeof(ucontext_t));
#endif
    qassert(swapcontext(t->return_context, t->context), 0);
#ifdef NEED_RLIMIT
    qthread_debug(ALL_DETAILS,
		  "qthread_exec(%p): setting stack size limits back to normal...\n",
		  t);
    if (!(t->flags & QTHREAD_REAL_MCCOY)) {
	rlp.rlim_cur = qlib->master_stack_size;
	qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
    }
#endif

    assert(t != NULL);
    assert(c != NULL);

    qthread_debug(ALL_DETAILS, "qthread_exec(%p): finished\n", t);
}				       /*}}} */

/* this function yields thread t to the master kernel thread */
void qthread_yield(qthread_t * t)
{				       /*{{{ */
    if (t == NULL) {
	t = qthread_self();
    }
    if (t != NULL) {
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_yield(): tid %u yielding...\n", t->thread_id);
	t->thread_state = QTHREAD_STATE_YIELDED;
	qthread_back_to_master(t);
	qthread_debug(THREAD_BEHAVIOR, "qthread_yield(): tid %u resumed.\n",
		      t->thread_id);
    }
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

    qthread_debug(THREAD_BEHAVIOR, "qthread_fork(%p, %p, %p)\n", f, arg, ret);
    assert(qlib);
    assert(myshep);
    if (myshep) {		       /* note: for forking from a qthread, NO LOCKS! */
	int loopctr = 0;

	do {
	    shep = (qthread_shepherd_id_t) (myshep->sched_shepherd++);
	    if (myshep->sched_shepherd == qlib->nshepherds) {
		myshep->sched_shepherd = 0;
	    }
	    loopctr++;
	} while (QTHREAD_CASLOCK_READ_UI(qlib->shepherds[shep].active) != 1 &&
		 loopctr <= qlib->nshepherds);
	if (loopctr > qlib->nshepherds) {
	    qthread_debug(THREAD_BEHAVIOR, "qthread_fork(): could not find an active shepherd\n");
	    return QTHREAD_NOT_ALLOWED;
	}
    } else {
	shep = (qthread_shepherd_id_t)
	    qthread_internal_incr_mod(&qlib->sched_shepherd, qlib->nshepherds,
				      &qlib->sched_shepherd_lock);
	assert(shep < qlib->nshepherds);
    }
    t = qthread_thread_new(f, arg, ret, shep);
    if (t) {
	qthread_debug(THREAD_BEHAVIOR, "qthread_fork(): new-tid %u shep %u\n",
		      t->thread_id, shep);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	qt_lfqueue_enqueue(qlib->shepherds[shep].ready, t, myshep);
	return QTHREAD_SUCCESS;
    }
    qthread_debug(THREAD_BEHAVIOR, "qthread_fork(): malloc error\n");
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

int qthread_fork_to(const qthread_f f, const void *arg, aligned_t * ret,
		    const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    assert(shepherd < qlib->nshepherds);
    assert(f != NULL);
    if (shepherd >= qlib->nshepherds || f == NULL) {
	return QTHREAD_BADARGS;
    }
    t = qthread_thread_new(f, arg, ret, shepherd);
    t->target_shepherd = &(qlib->shepherds[shepherd]);
    if (t) {
	qthread_shepherd_t *shep = &(qlib->shepherds[shepherd]);
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_fork_to(): new-tid %u shep %u\n", t->thread_id,
		      shepherd);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	if (QTHREAD_CASLOCK_READ_UI(shep->active) == 0) {
	    shep = qthread_find_active_shepherd(shep->sorted_sheplist, shep->shep_dists);
	}
	t->shepherd_ptr = shep;
	qt_lfqueue_enqueue(shep->ready, t,
			   (qthread_shepherd_t *)
			   pthread_getspecific(shepherd_structs));
	return QTHREAD_SUCCESS;
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

int qthread_fork_future_to(const qthread_t * me, const qthread_f f,
			   const void *arg, aligned_t * ret,
			   const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    qthread_t *t;

    assert(shepherd < qlib->nshepherds);
    assert(f != NULL);
    if (shepherd >= qlib->nshepherds || f == NULL) {
	return QTHREAD_BADARGS;
    }
    if (QTHREAD_CASLOCK_READ_UI(qlib->shepherds[shepherd].active) != 1) {
	return QTHREAD_NOT_ALLOWED;
    }
    t = qthread_thread_new(f, arg, ret, shepherd);
    if (t) {
	qthread_shepherd_t *shep = &(qlib->shepherds[shepherd]);

	t->flags |= QTHREAD_FUTURE;
	t->target_shepherd = &(qlib->shepherds[shepherd]);
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_fork_future_to(): new-tid %u shep %u\n",
		      t->thread_id, shepherd);

	if (ret) {
	    int test = qthread_empty(qthread_self(), ret);

	    if (test != QTHREAD_SUCCESS) {
		qthread_thread_free(t);
		return test;
	    }
	}
	if (QTHREAD_CASLOCK_READ_UI(shep->active) == 0) {
	    shep =
		qthread_find_active_shepherd(shep->sorted_sheplist,
					     shep->shep_dists);
	}
	t->shepherd_ptr = shep;
	qt_lfqueue_enqueue(shep->ready, t, me->shepherd_ptr);
	return QTHREAD_SUCCESS;
    }
    return QTHREAD_MALLOC_ERROR;
}				       /*}}} */

static QINLINE void qthread_back_to_master(qthread_t * t)
{				       /*{{{ */
#ifdef NEED_RLIMIT
    struct rlimit rlp;

    qthread_debug(ALL_DETAILS,
		  "qthread_back_to_master(%p): setting stack size limits for master thread...\n",
		  t);
    if (!(t->flags & QTHREAD_REAL_MCCOY)) {
	rlp.rlim_cur = qlib->master_stack_size;
	rlp.rlim_max = qlib->max_stack_size;
	qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
    }
#endif
    /* now back to your regularly scheduled master thread */
#ifdef QTHREAD_USE_VALGRIND
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(t->context, sizeof(ucontext_t));
    VALGRIND_CHECK_MEM_IS_ADDRESSABLE(t->return_context, sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(t->context, sizeof(ucontext_t));
    VALGRIND_MAKE_MEM_DEFINED(t->return_context, sizeof(ucontext_t));
#endif
    qassert(swapcontext(t->context, t->return_context), 0);
#ifdef NEED_RLIMIT
    qthread_debug(ALL_DETAILS,
		  "qthread_back_to_master(%p): setting stack size limits back to qthread size...\n",
		  t);
    if (!(t->flags & QTHREAD_REAL_MCCOY)) {
	rlp.rlim_cur = qlib->qthread_stack_size;
	qassert(setrlimit(RLIMIT_STACK, &rlp), 0);
    }
#endif
}				       /*}}} */

qthread_t *qthread_prepare(const qthread_f f, const void *arg,
			   aligned_t * ret)
{				       /*{{{ */
    qthread_t *t;
    qthread_shepherd_id_t shep;
    qthread_shepherd_t *myshep =
	(qthread_shepherd_t *) pthread_getspecific(shepherd_structs);

    assert(myshep);
    if (myshep) {
	int loopctr = 0;

	do {
	    shep = (qthread_shepherd_id_t) (myshep->sched_shepherd++);
	    if (myshep->sched_shepherd == qlib->nshepherds) {
		myshep->sched_shepherd = 0;
	    }
	    loopctr++;
	} while (QTHREAD_CASLOCK_READ_UI(qlib->shepherds[shep].active) != 1 &&
		 loopctr <= qlib->nshepherds);
	if (loopctr > qlib->nshepherds) {
	    return NULL;
	}
    } else {
	shep = (qthread_shepherd_id_t)
	    qthread_internal_incr_mod(&qlib->sched_shepherd, qlib->nshepherds,
				      &qlib->sched_shepherd_lock);
	assert(shep < qlib->nshepherds);
    }

    t = qthread_thread_bare(f, arg, ret, shep);
    qthread_debug(THREAD_BEHAVIOR, "qthread_prepare(): new-tid %u shep %u\n",
		  t->thread_id, shep);
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

    qthread_debug(THREAD_BEHAVIOR,
		  "qthread_prepare_for(): new-tid %u shep %u\n", t->thread_id,
		  shepherd);
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
    int ret;

    if (QTHREAD_CASLOCK_READ_UI(t->shepherd_ptr->active) != 1) {
	return QTHREAD_NOT_ALLOWED;
    }
    ret = qthread_thread_plush(t);
    if (ret == QTHREAD_SUCCESS) {
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_schedule(): new-tid %u shep %u\n",
		      t->thread_id, t->shepherd_ptr->shepherd_id);
	qt_lfqueue_enqueue(t->shepherd_ptr->ready, t, (qthread_shepherd_t *)
			   pthread_getspecific(shepherd_structs));
    }
    return ret;
}				       /*}}} */

int qthread_schedule_on(qthread_t * t, const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    int ret;

    if (QTHREAD_CASLOCK_READ_UI(qlib->shepherds[shepherd].active) != 1) {
	return QTHREAD_NOT_ALLOWED;
    }
    ret = qthread_thread_plush(t);
    if (ret == QTHREAD_SUCCESS) {
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_schedule_on(): new-tid %u shep %u\n",
		      t->thread_id, shepherd);
	qt_lfqueue_enqueue(qlib->shepherds[shepherd].ready, t,
			   (qthread_shepherd_t *)
			   pthread_getspecific(shepherd_structs));
    }
    return ret;
}				       /*}}} */

/* function to move a qthread from one shepherd to another */
int qthread_migrate_to(qthread_t * me, const qthread_shepherd_id_t shepherd)
{				       /*{{{ */
    if (me == NULL) {
	me = qthread_self();
    }
    assert(me == qthread_self());
    if (me->shepherd_ptr->shepherd_id == shepherd) {
	me->target_shepherd = me->shepherd_ptr;
	return QTHREAD_SUCCESS;
    }
    if (me->flags & QTHREAD_REAL_MCCOY) {
	return QTHREAD_NOT_ALLOWED;
    }
    if (me && shepherd < qlib->nshepherds) {
	qthread_debug(THREAD_BEHAVIOR,
		      "qthread_migrate_to(): tid %u from shep %u to shep %u\n",
		      me->thread_id, me->shepherd_ptr->shepherd_id, shepherd);
	me->target_shepherd = &(qlib->shepherds[shepherd]);
	me->thread_state = QTHREAD_STATE_MIGRATING;
	me->blockedon = (struct qthread_lock_s *)(intptr_t) shepherd;
	qthread_back_to_master(me);

	qthread_debug(THREAD_DETAILS,
		      "qthread_migrate_to(): tid %u awakes on shepherd %u!\n",
		      me->thread_id, me->shepherd_ptr->shepherd_id);
	return QTHREAD_SUCCESS;
    } else {
	return QTHREAD_BADARGS;
    }
}				       /*}}} */

/* functions to implement FEB locking/unlocking */

/* The lock ordering in these functions is very particular, and is designed to
 * reduce the impact of having only one hashtable. Don't monkey with it unless
 * you REALLY know what you're doing! If one hashtable becomes a problem, we
 * may need to move to a new mechanism.
 */

/* This is just a little function that should help in debugging */
int qthread_feb_status(const aligned_t * addr)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const aligned_t *alignedaddr;
    int status = 1;		/* full */
    const int lockbin = QTHREAD_CHOOSE_STRIPE(addr);

    QALIGN(addr, alignedaddr, "qthread_feb_status()");
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]); {
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (m) {
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	    status = m->full;
	    QTHREAD_UNLOCK(&m->lock);
	    REPORTUNLOCK(m);
	}
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);
    qthread_debug(LOCK_BEHAVIOR, "qthread_feb_status(): addr %p is %i", addr,
		  status);
    return status;
}				       /*}}} */

/* This allocates a new, initialized addrstat structure, which is used for
 * keeping track of the FEB status of an address. It expects a shepherd pointer
 * to use to find the right memory pool to use. */
static QINLINE qthread_addrstat_t *qthread_addrstat_new(qthread_shepherd_t *
							shepherd)
{				       /*{{{ */
    qthread_addrstat_t *ret = ALLOC_ADDRSTAT(shepherd);

    if (ret != NULL) {
	if (pthread_mutex_init(&ret->lock, NULL) != 0) {
	    FREE_ADDRSTAT(ret);
	    return NULL;
	}
	ret->full = 1;
	ret->EFQ = NULL;
	ret->FEQ = NULL;
	ret->FFQ = NULL;
	QTHREAD_EMPTY_TIMER_INIT(ret);
    }
    return ret;
}				       /*}}} */

/* this function removes the FEB data structure for the address maddr from the
 * hash table */
static QINLINE void qthread_FEB_remove(void *maddr)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(maddr);

    qthread_debug(LOCK_DETAILS,
		  "qthread_FEB_remove(): attempting removal %p\n", maddr);
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]); {
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin], maddr);
	if (m) {
	    QTHREAD_LOCK(&(m->lock));
	    REPORTLOCK(m);
	    if (m->FEQ == NULL && m->EFQ == NULL && m->FFQ == NULL &&
		m->full == 1) {
		qthread_debug(LOCK_DETAILS,
			      "qthread_FEB_remove(): all lists are empty, and status is full\n");
		qt_hash_remove(qlib->FEBs[lockbin], maddr);
	    } else {
		QTHREAD_UNLOCK(&(m->lock));
		REPORTUNLOCK(m);
		qthread_debug(LOCK_DETAILS,
			      "qthread_FEB_remove(): address cannot be removed; in use\n");
		m = NULL;
	    }
	}
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);
    if (m != NULL) {
	QTHREAD_UNLOCK(&m->lock);
	REPORTUNLOCK(m);
	qthread_addrstat_delete(m);
    }
}				       /*}}} */

static QINLINE void qthread_gotlock_empty(qthread_shepherd_t * shep,
					  qthread_addrstat_t * m, void *maddr,
					  const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;

    m->full = 0;
    QTHREAD_EMPTY_TIMER_START(m);
    if (m->EFQ != NULL) {
	/* dQ */
	X = m->EFQ;
	m->EFQ = X->next;
	/* op */
	if (maddr && maddr != X->addr) {
	    memcpy(maddr, X->addr, sizeof(aligned_t));
	}
	/* requeue */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	qt_lfqueue_enqueue(X->waiter->shepherd_ptr->ready, X->waiter, shep);
	FREE_ADDRRES(X);
	qthread_gotlock_fill(shep, m, maddr, 1);
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

static QINLINE void qthread_gotlock_fill(qthread_shepherd_t * shep,
					 qthread_addrstat_t * m, void *maddr,
					 const char recursive)
{				       /*{{{ */
    qthread_addrres_t *X = NULL;
    int removeable;

    qthread_debug(LOCK_DETAILS, "qthread_gotlock_fill(%p, %p)\n", m, maddr);
    m->full = 1;
    QTHREAD_EMPTY_TIMER_STOP(m);
    /* dequeue all FFQ, do their operation, and schedule them */
    qthread_debug(LOCK_DETAILS, "qthread_gotlock_fill(): dQ all FFQ\n");
    while (m->FFQ != NULL) {
	/* dQ */
	X = m->FFQ;
	m->FFQ = X->next;
	/* op */
	if (X->addr && X->addr != maddr) {
	    memcpy(X->addr, maddr, sizeof(aligned_t));
	}
	/* schedule */
	X->waiter->thread_state = QTHREAD_STATE_RUNNING;
	qt_lfqueue_enqueue(X->waiter->shepherd_ptr->ready, X->waiter, shep);
	FREE_ADDRRES(X);
    }
    if (m->FEQ != NULL) {
	/* dequeue one FEQ, do their operation, and schedule them */
	qthread_t *waiter;

	qthread_debug(LOCK_DETAILS, "qthread_gotlock_fill(): dQ 1 FEQ\n");
	X = m->FEQ;
	m->FEQ = X->next;
	/* op */
	if (X->addr && X->addr != maddr) {
	    memcpy(X->addr, maddr, sizeof(aligned_t));
	}
	waiter = X->waiter;
	waiter->thread_state = QTHREAD_STATE_RUNNING;
	qt_lfqueue_enqueue(waiter->shepherd_ptr->ready, waiter, shep);
	FREE_ADDRRES(X);
	qthread_gotlock_empty(shep, m, maddr, 1);
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

int qthread_empty(qthread_t * me, const aligned_t * dest)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(dest);

    QALIGN(dest, alignedaddr, "qthread_empty()");
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]);
    {				       /* BEGIN CRITICAL SECTION */
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (!m) {
	    /* currently full, and must be added to the hash to empty */
	    m = qthread_addrstat_new(me ? (me->shepherd_ptr) :
				     pthread_getspecific(shepherd_structs));
	    if (!m) {
		qt_hash_unlock(qlib->FEBs[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    m->full = 0;
	    QTHREAD_EMPTY_TIMER_START(m);
	    qt_hash_put(qlib->FEBs[lockbin], (void *)alignedaddr, m);
	    m = NULL;
	} else {
	    /* it could be either full or not, don't know */
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
    }				       /* END CRITICAL SECTION */
    qt_hash_unlock(qlib->FEBs[lockbin]);
    qthread_debug(LOCK_BEHAVIOR, "qthread_empty(): %p is now empty\n", dest);
    if (m) {
	qthread_gotlock_empty(me->shepherd_ptr, m, (void *)alignedaddr, 0);
    }
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_fill(qthread_t * me, const aligned_t * dest)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(dest);

    QALIGN(dest, alignedaddr, "qthread_fill()");
    /* lock hash */
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]);
    {				       /* BEGIN CRITICAL SECTION */
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (m) {
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
    }				       /* END CRITICAL SECTION */
    qt_hash_unlock(qlib->FEBs[lockbin]);	/* unlock hash */
    qthread_debug(LOCK_BEHAVIOR, "qthread_fill(): %p is now full\n", dest);
    if (m) {
	/* if dest wasn't in the hash, it was already full. Since it was,
	 * we need to fill it. */
	qthread_gotlock_fill(me->shepherd_ptr, m, (void *)alignedaddr, 0);
    }
    return QTHREAD_SUCCESS;
}				       /*}}} */

/* the way this works is that:
 * 1 - data is copies from src to destination
 * 2 - the destination's FEB state gets changed from empty to full
 */

int qthread_writeF(qthread_t * me, aligned_t * restrict const dest,
		   const aligned_t * restrict const src)
{				       /*{{{ */
    qthread_addrstat_t *m;
    aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(dest);

    if (me == NULL) {
	me = qthread_self();
    }
    qthread_debug(LOCK_BEHAVIOR,
		  "qthread_writeF(): tid %u dest=%p src=%p...\n",
		  me->thread_id, dest, src);
    QALIGN(dest, alignedaddr, "qthread_fill_with()");
    QTHREAD_LOCK_UNIQUERECORD(feb, dest, me);
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]); {	/* lock hash */
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (!m) {
	    m = qthread_addrstat_new(me->shepherd_ptr);
	    if (!m) {
		qt_hash_unlock(qlib->FEBs[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    qt_hash_put(qlib->FEBs[lockbin], alignedaddr, m);
	}
	QTHREAD_LOCK(&m->lock);
	REPORTLOCK(m);
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);	/* unlock hash */
    /* we have the lock on m, so... */
    if (dest && dest != src) {
	memcpy(dest, src, sizeof(aligned_t));
    }
    qthread_debug(LOCK_BEHAVIOR,
		  "qthread_writeF(): tid %u succeeded on %p=%p\n",
		  me->thread_id, dest, src);
    qthread_gotlock_fill(me->shepherd_ptr, m, alignedaddr, 0);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_writeF_const(qthread_t * me, aligned_t * const dest,
			 const aligned_t src)
{				       /*{{{ */
    return qthread_writeF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state gets changed from empty to full
 */

int qthread_writeEF(qthread_t * me, aligned_t * restrict const dest,
		    const aligned_t * restrict const src)
{				       /*{{{ */
    qthread_addrstat_t *m;
    qthread_addrres_t *X = NULL;
    aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(dest);

    QTHREAD_LOCK_TIMER_DECLARATION(febblock);

    if (me == NULL) {
	me = qthread_self();
    }
    qthread_debug(LOCK_BEHAVIOR,
		  "qthread_writeEF(): tid %u dest=%p src=%p...\n",
		  me->thread_id, dest, src);
    QTHREAD_LOCK_UNIQUERECORD(feb, dest, me);
    QTHREAD_LOCK_TIMER_START(febblock);
    QALIGN(dest, alignedaddr, "qthread_writeEF()");
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]);
    {
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (!m) {
	    m = qthread_addrstat_new(me->shepherd_ptr);
	    if (!m) {
		qt_hash_unlock(qlib->FEBs[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    qt_hash_put(qlib->FEBs[lockbin], alignedaddr, m);
	}
	QTHREAD_LOCK(&(m->lock));
	REPORTLOCK(m);
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);
    qthread_debug(LOCK_DETAILS, "qthread_writeEF(): data structure locked\n");
    /* by this point m is locked */
    qthread_debug(LOCK_DETAILS, "qthread_writeEF(): m->full == %i\n",
		  m->full);
    if (m->full == 1) {		       /* full, thus, we must block */
	QTHREAD_WAIT_TIMER_DECLARATION;
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
	qthread_debug(LOCK_DETAILS, "qthread_writeEF(): back to parent\n");
	me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	me->blockedon = (struct qthread_lock_s *)m;
	QTHREAD_WAIT_TIMER_START();
	qthread_back_to_master(me);
	QTHREAD_WAIT_TIMER_STOP(me, febwait);
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_writeFE(): tid %u succeeded on %p=%p after waiting\n",
		      me->thread_id, dest, src);
    } else {
	if (dest && dest != src) {
	    memcpy(dest, src, sizeof(aligned_t));
	}
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_writeFE(): tid %u succeeded on %p=%p\n",
		      me->thread_id, dest, src);
	qthread_gotlock_fill(me->shepherd_ptr, m, alignedaddr, 0);
    }
    QTHREAD_LOCK_TIMER_STOP(febblock, me);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_writeEF_const(qthread_t * me, aligned_t * const dest,
			  const aligned_t src)
{				       /*{{{ */
    return qthread_writeEF(me, dest, &src);
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 */

int qthread_readFF(qthread_t * me, aligned_t * restrict const dest,
		   const aligned_t * restrict const src)
{				       /*{{{ */
    qthread_addrstat_t *m = NULL;
    qthread_addrres_t *X = NULL;
    const aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(src);

    QTHREAD_LOCK_TIMER_DECLARATION(febblock);

    if (me == NULL) {
	me = qthread_self();
    }
    qthread_debug(LOCK_BEHAVIOR,
		  "qthread_readFF(): tid %u dest=%p src=%p...\n",
		  me->thread_id, dest, src);
    QTHREAD_LOCK_UNIQUERECORD(feb, src, me);
    QTHREAD_LOCK_TIMER_START(febblock);
    QALIGN(src, alignedaddr, "qthread_readFF()");
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]);
    {
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       (void *)alignedaddr);
	if (!m) {
	    if (dest && dest != src) {
		memcpy(dest, src, sizeof(aligned_t));
	    }
	} else {
	    QTHREAD_LOCK(&m->lock);
	    REPORTLOCK(m);
	}
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);
    qthread_debug(LOCK_DETAILS, "qthread_readFF(): data structure locked\n");
    /* now m, if it exists, is locked - if m is NULL, then we're done! */
    if (m == NULL) {		       /* already full! */
	if (dest && dest != src) {
	    memcpy(dest, src, sizeof(aligned_t));
	}
    } else if (m->full != 1) {	       /* not full... so we must block */
	QTHREAD_WAIT_TIMER_DECLARATION;
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
	qthread_debug(LOCK_DETAILS, "qthread_readFF(): back to parent\n");
	me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	me->blockedon = (struct qthread_lock_s *)m;
	QTHREAD_WAIT_TIMER_START();
	qthread_back_to_master(me);
	QTHREAD_WAIT_TIMER_STOP(me, febwait);
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_readFF(): tid %u succeeded on %p=%p after waiting\n",
		      me->thread_id, dest, src);
    } else {			       /* exists AND is empty... weird, but that's life */
	if (dest && dest != src) {
	    memcpy(dest, src, sizeof(aligned_t));
	}
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_readFF(): tid %u succeeded on %p=%p\n",
		      me->thread_id, dest, src);
	QTHREAD_UNLOCK(&m->lock);
	REPORTUNLOCK(m);
    }
    QTHREAD_LOCK_TIMER_STOP(febblock, me);
    return QTHREAD_SUCCESS;
}				       /*}}} */

/* the way this works is that:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 * 3 - the src's FEB bits get changed from full to empty
 */

int qthread_readFE(qthread_t * me, aligned_t * restrict const dest,
		   const aligned_t * restrict const src)
{				       /*{{{ */
    qthread_addrstat_t *m;
    const aligned_t *alignedaddr;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(src);

    QTHREAD_LOCK_TIMER_DECLARATION(febblock);

    if (me == NULL) {
	me = qthread_self();
    }
    qthread_debug(LOCK_BEHAVIOR,
		  "qthread_readFE(): tid %u dest=%p src=%p...\n",
		  me->thread_id, dest, src);
    QTHREAD_LOCK_UNIQUERECORD(feb, src, me);
    QTHREAD_LOCK_TIMER_START(febblock);
    QALIGN(src, alignedaddr, "qthread_readFE()");
    QTHREAD_COUNT_THREADS_BINCOUNTER(febs, lockbin);
    qt_hash_lock(qlib->FEBs[lockbin]);
    {
	m = (qthread_addrstat_t *) qt_hash_get(qlib->FEBs[lockbin],
					       alignedaddr);
	if (!m) {
	    m = qthread_addrstat_new(me->shepherd_ptr);
	    if (!m) {
		qt_hash_unlock(qlib->FEBs[lockbin]);
		return QTHREAD_MALLOC_ERROR;
	    }
	    qt_hash_put(qlib->FEBs[lockbin], alignedaddr, m);
	}
	QTHREAD_LOCK(&(m->lock));
	REPORTLOCK(m);
    }
    qt_hash_unlock(qlib->FEBs[lockbin]);
    qthread_debug(LOCK_DETAILS, "qthread_readFE(): data structure locked\n");
    /* by this point m is locked */
    if (m->full == 0) {		       /* empty, thus, we must block */
	QTHREAD_WAIT_TIMER_DECLARATION;
	qthread_addrres_t *X = ALLOC_ADDRRES(me->shepherd_ptr);

	if (X == NULL) {
	    QTHREAD_UNLOCK(&m->lock);
	    REPORTUNLOCK(m);
	    return QTHREAD_MALLOC_ERROR;
	}
	X->addr = (aligned_t *) dest;
	X->waiter = me;
	X->next = m->FEQ;
	m->FEQ = X;
	qthread_debug(LOCK_DETAILS, "qthread_readFE(): back to parent\n");
	me->thread_state = QTHREAD_STATE_FEB_BLOCKED;
	/* so that the shepherd will unlock it */
	me->blockedon = (struct qthread_lock_s *)m;
	QTHREAD_WAIT_TIMER_START();
	qthread_back_to_master(me);
	QTHREAD_WAIT_TIMER_STOP(me, febwait);
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_readFF(): tid %u succeeded on %p=%p after waiting\n",
		      me->thread_id, dest, src);
    } else {			       /* full, thus IT IS OURS! MUAHAHAHA! */
	if (dest && dest != src) {
	    memcpy(dest, src, sizeof(aligned_t));
	}
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_readFF(): tid %u succeeded on %p=%p\n",
		      me->thread_id, dest, src);
	qthread_gotlock_empty(me->shepherd_ptr, m, (void*)alignedaddr, 0);
    }
    QTHREAD_LOCK_TIMER_STOP(febblock, me);
    return QTHREAD_SUCCESS;
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
 * reduce the impact of having a centralized hashtable. Don't monkey with it
 * unless you REALLY know what you're doing!
 */

int qthread_lock(qthread_t * me, const aligned_t * a)
{				       /*{{{ */
    qthread_lock_t *m;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(a);

    QTHREAD_LOCK_TIMER_DECLARATION(aquirelock);

    if (me == NULL) {
	me = qthread_self();
    }
    qthread_debug(LOCK_BEHAVIOR, "qthread_lock(%u, %p): starting...\n",
		  me->thread_id, a);
    QTHREAD_LOCK_UNIQUERECORD(lock, a, me);
    QTHREAD_LOCK_TIMER_START(aquirelock);

    QTHREAD_COUNT_THREADS_BINCOUNTER(locks, lockbin);
    qt_hash_lock(qlib->locks[lockbin]);
    m = (qthread_lock_t *) qt_hash_get(qlib->locks[lockbin], (void *)a);
    if (m == NULL) {
	m = ALLOC_LOCK(me->shepherd_ptr);
	if (m == NULL) {
	    qt_hash_unlock(qlib->locks[lockbin]);
	    return QTHREAD_MALLOC_ERROR;
	}
	assert(me->shepherd_ptr == (qthread_shepherd_t *)
	       pthread_getspecific(shepherd_structs));
	m->waiting = qthread_queue_new(me->shepherd_ptr);
	if (m->waiting == NULL) {
	    FREE_LOCK(m);
	    qt_hash_unlock(qlib->locks[lockbin]);
	    return QTHREAD_MALLOC_ERROR;
	}
	if (pthread_mutex_init(&m->lock, NULL) != 0) {
	    qthread_queue_free(m->waiting);
	    FREE_LOCK(m);
	    qt_hash_unlock(qlib->locks[lockbin]);
	    return QTHREAD_PTHREAD_ERROR;
	}
	QTHREAD_HOLD_TIMER_INIT(m);
	qt_hash_put(qlib->locks[lockbin], (void *)a, m);
	/* since we just created it, we own it */
	QTHREAD_LOCK(&m->lock);
	/* can only unlock the hash after we've locked the address, because
	 * otherwise there's a race condition: the address could be removed
	 * before we have a chance to add ourselves to it */
	qt_hash_unlock(qlib->locks[lockbin]);

#ifdef QTHREAD_DEBUG
	m->owner = me->thread_id;
#endif
	QTHREAD_UNLOCK(&m->lock);
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_lock(%u, %p): returned (wasn't locked)\n",
		      me->thread_id, a);
    } else {
	QTHREAD_WAIT_TIMER_DECLARATION;
	/* success==failure: because it's in the hash, someone else owns
	 * the lock; dequeue this thread and yield. NOTE: it's up to the
	 * master thread to enqueue this thread and unlock the address
	 */
	QTHREAD_LOCK(&m->lock);
	/* for an explanation of the lock/unlock ordering here, see above */
	qt_hash_unlock(qlib->locks[lockbin]);

	me->thread_state = QTHREAD_STATE_BLOCKED;
	me->blockedon = m;

	QTHREAD_WAIT_TIMER_START();

	qthread_back_to_master(me);

	QTHREAD_WAIT_TIMER_STOP(me, lockwait);

	/* once I return to this context, I own the lock! */
	/* conveniently, whoever unlocked me already set up everything too */
	qthread_debug(LOCK_BEHAVIOR,
		      "qthread_lock(%u, %p): returned (was locked)\n",
		      me->thread_id, a);
    }
    QTHREAD_LOCK_TIMER_STOP(aquirelock, me);
    QTHREAD_HOLD_TIMER_START(m);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qthread_unlock(qthread_t * me, const aligned_t * a)
{				       /*{{{ */
    qthread_lock_t *m;
    qthread_t *u;
    const int lockbin = QTHREAD_CHOOSE_STRIPE(a);

    qthread_debug(LOCK_BEHAVIOR, "qthread_unlock(%u, %p)\n", me->thread_id,
		  a);

    QTHREAD_COUNT_THREADS_BINCOUNTER(locks, lockbin);
    qt_hash_lock(qlib->locks[lockbin]);
    m = (qthread_lock_t *) qt_hash_get(qlib->locks[lockbin], (void *)a);
    if (m == NULL) {
	/* unlocking an address that's already unlocked */
	qt_hash_unlock(qlib->locks[lockbin]);
	return QTHREAD_REDUNDANT;
    }
    QTHREAD_LOCK(&m->lock);

    QTHREAD_HOLD_TIMER_STOP(m, me->shepherd_ptr);

    /* unlock the address... if anybody's waiting for it, give them the lock
     * and put them in a ready queue.  If not, delete the lock structure.
     */

    QTHREAD_LOCK(&m->waiting->lock);
    u = qthread_dequeue_nonblocking(m->waiting);
    if (u == NULL) {
	qthread_debug(LOCK_DETAILS,
		      "qthread_unlock(%u,%p): deleting waiting queue\n",
		      me->thread_id, a);
	qt_hash_remove(qlib->locks[lockbin], (void *)a);
	qt_hash_unlock(qlib->locks[lockbin]);
	QTHREAD_HOLD_TIMER_DESTROY(m);
	QTHREAD_UNLOCK(&m->waiting->lock);
	qthread_queue_free(m->waiting);
	QTHREAD_UNLOCK(&m->lock);
	QTHREAD_DESTROYLOCK(&m->lock);
	FREE_LOCK(m);
    } else {
	qt_hash_unlock(qlib->locks[lockbin]);
	qthread_debug(LOCK_DETAILS,
		      "qthread_unlock(%u,%p): pulling thread from queue (%p)\n",
		      me->thread_id, a, u);
	u->thread_state = QTHREAD_STATE_RUNNING;
#ifdef QTHREAD_DEBUG
	m->owner = u->thread_id;
#endif

	/* NOTE: because of the use of getcontext()/setcontext(), threads
	 * return to the shepherd that setcontext()'d into them, so they
	 * must remain in that queue.
	 */
	qt_lfqueue_enqueue(u->shepherd_ptr->ready, u, me->shepherd_ptr);

	QTHREAD_UNLOCK(&m->waiting->lock);
	QTHREAD_UNLOCK(&m->lock);
    }

    return QTHREAD_SUCCESS;
}				       /*}}} */

/* These are just accessor functions */
unsigned qthread_id(const qthread_t * t)
{				       /*{{{ */
    qthread_debug(ALL_CALLS, "qthread_id(%u)",
		  t ? t->thread_id : (unsigned)-1);
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
			      &qlib->max_thread_id_lock, 1);
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

int qthread_shep_ok(const qthread_t * t)
{
    qthread_shepherd_t *ret;

    if (t) {
	return QTHREAD_CASLOCK_READ_UI(t->shepherd_ptr->active);
    }
    ret = (qthread_shepherd_t *) pthread_getspecific(shepherd_structs);
    if (ret == NULL) {
	return QTHREAD_PTHREAD_ERROR;
    } else {
	return QTHREAD_CASLOCK_READ_UI(ret->active);
    }
}

unsigned int qthread_internal_shep_to_node(const qthread_shepherd_id_t shep)
{				       /*{{{ */
    return qlib->shepherds[shep].node;
}				       /*}}} */

/* returns the distance between two shepherds */
int qthread_distance(const qthread_shepherd_id_t src,
		     const qthread_shepherd_id_t dest)
{				       /*{{{ */
    assert(src < qlib->nshepherds);
    assert(dest < qlib->nshepherds);
    if (src >= qlib->nshepherds || dest >= qlib->nshepherds) {
	return QTHREAD_BADARGS;
    }
    if (qlib->shepherds[src].shep_dists == NULL) {
	return 0;
    } else {
	return qlib->shepherds[src].shep_dists[dest];
    }
}				       /*}}} */

/* returns a list of shepherds, sorted by their distance from this qthread;
 * if NULL, then all sheps are equidistant */
const qthread_shepherd_id_t *qthread_sorted_sheps(const qthread_t * t)
{				       /*{{{ */
    assert(t);
    if (t == NULL) {
	return NULL;
    }
    assert(t->shepherd_ptr);
    return t->shepherd_ptr->sorted_sheplist;
}				       /*}}} */

/* returns a list of shepherds, sorted by their distance from the specified shepherd;
 * if NULL, then all sheps are equidistant */
const qthread_shepherd_id_t *qthread_sorted_sheps_remote(const
							 qthread_shepherd_id_t
							 src)
{				       /*{{{ */
    assert(src < qlib->nshepherds);
    if (src >= qlib->nshepherds) {
	return NULL;
    }
    return qlib->shepherds[src].sorted_sheplist;
}				       /*}}} */

/* returns the number of shepherds (i.e. one more than the largest valid shepherd id) */
qthread_shepherd_id_t qthread_num_shepherds(void)
{				       /*{{{ */
    return (qthread_shepherd_id_t) (qlib->nshepherds);
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
