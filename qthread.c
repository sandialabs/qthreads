#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef NEED_RLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif
#include <cprops/mempool.h>
#include "qthread.h"

/* internal data structures */
typedef struct
{
    qthread_t *head;
    qthread_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t notempty;
} qthread_queue_t;

typedef struct qthread_shepherd_s
{
    pthread_t kthread;
    unsigned kthread_index;
    qthread_queue_t *ready;
} qthread_shepherd_t;

typedef struct qthread_lock_s
{
    void *address;
    unsigned owner;
    pthread_mutex_t lock;
    qthread_queue_t *waiting;
} qthread_lock_t;

/* internal globals */
static qlib_t *qlib = NULL;

static cp_mempool *qthread_pool = NULL;
static cp_mempool *context_pool = NULL;
static cp_mempool *stack_pool = NULL;
static cp_mempool *queue_pool = NULL;
static cp_mempool *lock_pool = NULL;

/* Internal functions */
static void *qthread_shepherd(void *arg);
static void qthread_wrapper(void *arg);

static inline qthread_t *qthread_thread_new(void (*f) (), void *arg);
static inline void qthread_thread_free(qthread_t * t);
static inline void qthread_stack_new(qthread_t * t, unsigned stack_size);
static inline void qthread_stack_free(qthread_t * t);

static inline qthread_queue_t *qthread_queue_new();
static inline void qthread_queue_free(qthread_queue_t * q);

static inline void qthread_enqueue(qthread_queue_t * q, qthread_t * t);
static inline qthread_t *qthread_dequeue(qthread_queue_t * q);
static inline qthread_t *qthread_dequeue_nonblocking(qthread_queue_t * q);

static inline void qthread_exec(qthread_t * t, ucontext_t * c);

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

static inline unsigned qthread_internal_atomic_check(unsigned *x,
						     pthread_mutex_t * lock)
{				       /*{{{ */
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    pthread_mutex_unlock(lock);

    return (r);
}				       /*}}} */

/* the qthread_shepherd() is the pthread responsible for actually
 * executing the work units
 */

/*#define QTHREAD_DEBUG 1*/
/* for debugging */
#ifdef QTHREAD_DEBUG
static inline void qthread_debug(char *format, ...)
{				       /*{{{ */
    static pthread_mutex_t output_lock;
    static int inited = 0;
    va_list args;

    if (inited == 0) {
	inited = 1;
	pthread_mutex_init(&output_lock, NULL);
    }
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

	if (t->thread_state == QTHREAD_STATE_TERM_SHEP)
	    done = 1;
	else {
	    assert((t->thread_state == QTHREAD_STATE_NEW) ||
		   (t->thread_state == QTHREAD_STATE_RUNNING));

	    assert(t->f != NULL);

	    /* note: there's a good argument that the following should
	     * be: (*t->f)(t), however the state management would be
	     * more complex 
	     */

	    t->shepherd = me->kthread_index;
	    qthread_exec(t, &my_context);
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
		    qthread_unlock(t, t);
		    break;
	    }
	}
    }

    qthread_debug("qthread_shepherd(%u): finished\n", me->kthread_index);
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
	 cp_hashtable_create(4, cp_hash_addr,
			     cp_hash_compare_addr)) == NULL) {
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
    assert(pthread_mutex_init(&qlib->sched_kthread_lock, NULL) == 0);
    assert(pthread_mutex_init(&qlib->max_thread_id_lock, NULL) == 0);

#ifdef NEED_RLIMIT
    assert(getrlimit(RLIMIT_STACK, &rlp) == 0);
    qthread_debug("stack sizes ... cur: %u max: %u\n", rlp.rlim_cur,
		  rlp.rlim_max);
    qlib->master_stack_size = rlp.rlim_cur;
    qlib->max_stack_size = rlp.rlim_max;
#endif

    /* set up the memory pools */
    qthread_pool = cp_mempool_create_by_option(0, sizeof(qthread_t), 10000);
    context_pool = cp_mempool_create_by_option(0, sizeof(ucontext_t), 10000);
    stack_pool =
	cp_mempool_create_by_option(0, qlib->qthread_stack_size, 10000);
    queue_pool = cp_mempool_create_by_option(0, sizeof(qthread_queue_t), 10);
    lock_pool = cp_mempool_create_by_option(0, sizeof(qthread_lock_t), 10);

    /* spawn the number of shepherd threads that were specified */
    for (i = 0; i < nkthreads; i++) {
	qlib->kthreads[i].kthread_index = i;
	qlib->kthreads[i].ready = qthread_queue_new();

	qthread_debug("qthread_init(): forking shepherd thread %p\n",
		      &qlib->kthreads[i]);

	if ((r =
	     pthread_create(&qlib->kthreads[i].kthread, NULL,
			    qthread_shepherd, &qlib->kthreads[i])) != 0) {
	    fprintf(stderr, "qthread_init: pthread_create() failed (%d)\n",
		    r);
	    abort();
	}
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
	t = qthread_thread_new(NULL, NULL);
	t->thread_state = QTHREAD_STATE_TERM_SHEP;
	qthread_enqueue(qlib->kthreads[i].ready, t);
    }

    /* wait for each thread to drain it's queue! */
    for (i = 0; i < qlib->nkthreads; i++) {
	if ((r = pthread_join(qlib->kthreads[i].kthread, NULL)) != 0) {
	    fprintf(stderr, "qthread_finalize: pthread_join() failed (%d)\n",
		    r);
	    abort();
	}
    }

    cp_hashtable_destroy(qlib->locks);

    assert(pthread_mutex_destroy(&qlib->sched_kthread_lock) == 0);
    assert(pthread_mutex_destroy(&qlib->max_thread_id_lock) == 0);

    cp_mempool_destroy(qthread_pool);
    cp_mempool_destroy(context_pool);
    cp_mempool_destroy(stack_pool);
    cp_mempool_destroy(queue_pool);
    cp_mempool_destroy(lock_pool);
    free(qlib->kthreads);
    free(qlib);
    qlib = NULL;

    qthread_debug("qthread_finalize(): finished.\n");
}				       /*}}} */

/************************************************************/
/* functions to manage thread stack allocation/deallocation */
/************************************************************/

static inline qthread_t *qthread_thread_new(void (*f) (), void *arg)
{				       /*{{{ */
    qthread_t *t;

    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    if ((t = (qthread_t *) cp_mempool_alloc(qthread_pool)) == NULL) {
	perror("qthread_thread_new()");
	abort();
    }

    if (((t->context) =
	 (ucontext_t *) cp_mempool_alloc(context_pool)) == NULL) {
	perror("qthread_thread_new()");
	abort();
    }

    t->thread_state = QTHREAD_STATE_NEW;
    t->f = f;
    t->arg = arg;
    t->stack = NULL;
    t->blockedon = NULL;

    return (t);
}				       /*}}} */

static inline void qthread_thread_free(qthread_t * t)
{				       /*{{{ */
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    assert(t != NULL);

    cp_mempool_free(context_pool, t->context);
    qthread_stack_free(t);

    cp_mempool_free(qthread_pool, t);
}				       /*}}} */


static inline void qthread_stack_new(qthread_t * t, unsigned stack_size)
{				       /*{{{ */
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    if ((t->stack = (void *)cp_mempool_alloc(stack_pool)) == NULL) {
	perror("qthread_thread_new()");
	abort();
    }
}				       /*}}} */

static inline void qthread_stack_free(qthread_t * t)
{				       /*{{{ */
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */
    if (t->stack != NULL)
	cp_mempool_free(stack_pool, t->stack);
}				       /*}}} */

/* functions to manage the thread queues */

static inline qthread_queue_t *qthread_queue_new()
{				       /*{{{ */
    qthread_queue_t *q;

    if ((q = (qthread_queue_t *) cp_mempool_alloc(queue_pool)) == NULL) {
	perror("qthread_queue_new()");
	abort();
    }

    q->head = NULL;
    q->tail = NULL;
    assert(pthread_mutex_init(&q->lock, NULL) == 0);
    assert(pthread_cond_init(&q->notempty, NULL) == 0);
    return (q);
}				       /*}}} */

static inline void qthread_queue_free(qthread_queue_t * q)
{				       /*{{{ */
    assert((q->head == NULL) && (q->tail == NULL));
    assert(pthread_mutex_destroy(&q->lock) == 0);
    assert(pthread_cond_destroy(&q->notempty) == 0);
    cp_mempool_free(queue_pool, q);
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

    assert(pthread_mutex_unlock(&q->lock) == 0);
    qthread_debug("qthread_enqueue(%p,%p): finished\n", q, t);
}				       /*}}} */

static inline qthread_t *qthread_dequeue(qthread_queue_t * q)
{				       /*{{{ */
    qthread_t *t;

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
    qthread_t *t;

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
	t->context->uc_stack.ss_sp = t->stack;
	t->context->uc_stack.ss_size = qlib->qthread_stack_size;
	t->context->uc_stack.ss_flags = 0;
	/* the makecontext man page (Linux) says: set the uc_link FIRST
	 * why? no idea */
	t->context->uc_link = c;       /* NULL pthread_exit() */

	qthread_debug("qthread_exec(): context is {%p, %d, %p}\n",
		      t->context->uc_stack.ss_sp,
		      t->context->uc_stack.ss_size, t->context->uc_link);
	makecontext(t->context, (void (*)(void))qthread_wrapper, 1, t);	/* the casting shuts gcc up */
    } else {
	t->context->uc_link = c;       /* NULL pthread_exit() */
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
#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    qthread_debug("qthread_yield(): thread %p yielding.\n", t);
    t->thread_state = QTHREAD_STATE_YIELDED;
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_yield(%p): setting stack size limits for master thread...\n",
	 t);
    rlp.rlim_cur = qlib->master_stack_size;
    rlp.rlim_max = qlib->max_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif
    /* back to your regularly scheduled master thread */
    if (swapcontext(t->context, t->return_context) != 0) {
	perror("qthread_yield(): swapcontext() failed");
	abort();
    }
#ifdef NEED_RLIMIT
    qthread_debug
	("qthread_yield(%p): setting stack size limits back to qthread size...\n",
	 t);
    rlp.rlim_cur = qlib->qthread_stack_size;
    assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif
    qthread_debug("qthread_yield(): thread %p resumed.\n", t);
}				       /*}}} */

/* fork a thread by putting it in somebody's work queue
 * NOTE: scheduling happens here
 */

qthread_t *qthread_fork(qthread_f f, void *arg)
{				       /*{{{ */
    qthread_t *t;
    unsigned shep, tid;

    t = qthread_thread_new(f, arg);    /* new thread struct sans stack */
    qthread_stack_new(t, qlib->qthread_stack_size);	/* fill in stack */

    qthread_debug("qthread_fork(): creating qthread %p with stack %p\n", t,
		  t->stack);

    /* figure out which queue to put the thread into */
    tid =
	qthread_internal_atomic_inc(&qlib->max_thread_id,
				    &qlib->max_thread_id_lock, 1);
    t->thread_id = tid;

    shep =
	qthread_internal_atomic_inc_mod(&qlib->sched_kthread,
					&qlib->sched_kthread_lock, 1,
					qlib->nkthreads);

    qthread_debug("qthread_fork(): tid %u shep %u\n", tid, shep);

    qthread_lock(t, t);

    qthread_enqueue(qlib->kthreads[shep].ready, t);

    return (t);
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
    return;
}				       /*}}} */

/* functions to implement FEB locking/unlocking 
 *
 * NOTE: these have not been profiled, and so may need tweaking for speed
 * (e.g. multiple hash tables, shortening critical section, etc.)
 */

int qthread_lock(qthread_t * t, void *a)
{				       /*{{{ */
    qthread_lock_t *m;

#ifdef NEED_RLIMIT
    struct rlimit rlp;
#endif

    cp_hashtable_wrlock(qlib->locks);
    m = (qthread_lock_t *) cp_hashtable_get(qlib->locks, a);
    if (m == NULL) {
	if ((m = (qthread_lock_t *) cp_mempool_alloc(lock_pool)) == NULL) {
	    perror("qthread_lock()");
	    abort();
	}
	m->waiting = qthread_queue_new();
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

#ifdef NEED_RLIMIT
	qthread_debug
	    ("qthread_lock(%p): setting stack size limits for master thread...\n",
	     t);
	rlp.rlim_cur = qlib->master_stack_size;
	rlp.rlim_max = qlib->max_stack_size;
	assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif
	/* now back to your regularly scheduled the master thread */
	if (swapcontext(t->context, t->return_context) != 0) {
	    perror("qthread_lock(): swapcontext() failed!");
	    abort();
	}
#ifdef NEED_RLIMIT
	qthread_debug
	    ("qthread_lock(%p): setting stack size limits back to qthread size...\n",
	     t);
	rlp.rlim_cur = qlib->qthread_stack_size;
	assert(setrlimit(RLIMIT_STACK, &rlp) == 0);
#endif

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
	fprintf(stderr,
		"qthread_unlock(%p,%p): attempt to unlock an address "
		"that is not locked!\n", (void *)t, a);
	abort();
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
	qthread_queue_free(m->waiting);

	assert(pthread_mutex_unlock(&m->lock) == 0);
	assert(pthread_mutex_destroy(&m->lock) == 0);
	cp_mempool_free(lock_pool, m);
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
unsigned qthread_id(qthread_t *t)
{
    return t->shepherd;
}

void * qthread_arg(qthread_t *t)
{
    return t->arg;
}
