#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* The API */
#include "qthread/qthread.h"

/* System Headers */
#include <stdlib.h>            /* for malloc() and free() */

/* Internal Headers */
#include "qt_hash.h"
#include "qthread_innards.h"
#include "qt_profiling.h"
#include "qt_blocking_structs.h"
#include "qt_qthread_struct.h"
#include "qt_threadqueues.h"
#include "qt_locks.h"
#include "qt_debug.h"

/* Types */
typedef enum bt {
    LOCK,
    UNLOCK
} blocker_type;
typedef struct {
    pthread_mutex_t lock;
    void           *a;
    blocker_type    type;
} qthread_lock_blocker_t;

/* Static (global) data structures */
qt_mpool generic_lock_pool  = NULL;
qt_mpool generic_queue_pool = NULL;

/* Function Prototypes */
static QINLINE qthread_queue_t *qthread_queue_new(void);
static QINLINE void             qthread_queue_free(qthread_queue_t *q);
static QINLINE qthread_t *      qthread_dequeue(qthread_queue_t *q);

#if defined(UNPOOLED_LOCKS) || defined(UNPOOLED)
# define ALLOC_LOCK() (qthread_lock_t *)malloc(sizeof(qthread_lock_t))
# define FREE_LOCK(t) free(t)
#else
# define ALLOC_LOCK() (qthread_lock_t *)qt_mpool_alloc(generic_lock_pool)
# define FREE_LOCK(t) qt_mpool_free(generic_lock_pool, t)
#endif /* if defined(UNPOOLED_LOCKS) || defined(UNPOOLED) */

#if defined(UNPOOLED_QUEUES) || defined(UNPOOLED)
# define ALLOC_QUEUE() (qthread_queue_t *)malloc(sizeof(qthread_queue_t))
# define FREE_QUEUE(t) free(t)
#else
# define ALLOC_QUEUE() (qthread_queue_t *)qt_mpool_alloc(generic_queue_pool)
# define FREE_QUEUE(t) qt_mpool_free(generic_queue_pool, t)
#endif /* if defined(UNPOOLED_QUEUES) || defined(UNPOOLED) */

#if !defined(UNPOOLED_QUEUES) && !defined(UNPOOLED)
static void qt_lock_subsystem_shutdown(void)
{
    qt_mpool_destroy(generic_queue_pool);
    qt_mpool_destroy(generic_lock_pool);
}

#endif

void INTERNAL qt_lock_subsystem_init(void)
{
#if !defined(UNPOOLED_QUEUES) && !defined(UNPOOLED)
    generic_queue_pool = qt_mpool_create(sizeof(qthread_queue_t));
    generic_lock_pool  = qt_mpool_create(sizeof(qthread_lock_t));
    qthread_internal_cleanup(qt_lock_subsystem_shutdown);
#endif
}

static aligned_t qthread_lock_blocker_thread(void *arg)
{                                      /*{{{ */
    qthread_lock_blocker_t *const restrict a = (qthread_lock_blocker_t *)arg;

    switch (a->type) {
        case LOCK:
            qthread_lock(a->a);
            break;
        case UNLOCK:
            qthread_unlock(a->a);
            break;
    }
    pthread_mutex_unlock(&(a->lock));
    return 0;
}                                      /*}}} */

static void qthread_lock_blocker_func(void        *addr,
                                      blocker_type t)
{   /*{{{*/
    qthread_lock_blocker_t args = { PTHREAD_MUTEX_INITIALIZER, addr, t };

    pthread_mutex_lock(&args.lock);
    qthread_fork(qthread_lock_blocker_thread, &args, NULL);
    pthread_mutex_lock(&args.lock);
    pthread_mutex_unlock(&args.lock);
    pthread_mutex_destroy(&args.lock);
} /*}}}*/

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

int API_FUNC qthread_lock(const aligned_t *a)
{                      /*{{{ */
    qthread_lock_t *m;
    const int       lockbin  = QTHREAD_CHOOSE_STRIPE(a);
    qthread_t      *me       = qthread_internal_self();
    uint_fast8_t    inserted = 0;


    QTHREAD_LOCK_TIMER_DECLARATION(aquirelock);

    assert(qthread_library_initialized);

    if (!me) {
        qthread_lock_blocker_func((void *)a, LOCK);
        return QTHREAD_SUCCESS;
    }
    qthread_debug(LOCK_CALLS, "tid(%u), a(%p): starting...\n",
                  me->thread_id, a);
    QTHREAD_LOCK_UNIQUERECORD(lock, a, me);
    QTHREAD_LOCK_TIMER_START(aquirelock);

    QTHREAD_COUNT_THREADS_BINCOUNTER(locks, lockbin);
#ifdef LOCK_FREE_FEBS
    do {
        m = (qthread_lock_t *)qt_hash_get(qlib->locks[lockbin], (void *)a);
        if (!m) {
            /* currently unlocked; need to lock it! */
            m = ALLOC_LOCK();
            if (!m) { return QTHREAD_MALLOC_ERROR; }
            m->waiting = qthread_queue_new();
            if (!m->waiting) {
                FREE_LOCK(m);
                return QTHREAD_MALLOC_ERROR;
            }
            QTHREAD_FASTLOCK_INIT(m->lock);
            QTHREAD_HOLD_TIMER_INIT(m);
            m->owner = me->thread_id;
            m->valid = 1;
            inserted = 1;
            QTHREAD_FASTLOCK_LOCK(&m->lock);
            if (!qt_hash_put(qlib->locks[lockbin], (void *)a, m)) {
                QTHREAD_HOLD_TIMER_DESTROY(m);
                qthread_queue_free(m->waiting);
                QTHREAD_FASTLOCK_UNLOCK(&m->lock);
                QTHREAD_FASTLOCK_DESTROY(m->lock);
                continue;
            }
            break;
        } else {
            /* someone else has it locked! */
            hazardous_ptr(0, m);
            if (m != qt_hash_get(qlib->locks[lockbin], (void*)a)) continue;
            if (!m->valid) { continue; }
            QTHREAD_FASTLOCK_LOCK(&m->lock);
            if (!m->valid) {
                QTHREAD_FASTLOCK_UNLOCK(&m->lock);
                continue;
            }
            break;
        }
    } while (1);
#else
    qt_hash_lock(qlib->locks[lockbin]);
    {
        m = (qthread_lock_t *)qt_hash_get_locked(qlib->locks[lockbin], (void *)a);
        if (!m) {
            m = ALLOC_LOCK();
            if (!m) {
                qt_hash_unlock(qlib->locks[lockbin]);
                return QTHREAD_MALLOC_ERROR;
            }
            m->waiting = qthread_queue_new();
            if (!m->waiting) {
                FREE_LOCK(m);
                return QTHREAD_MALLOC_ERROR;
            }
            QTHREAD_FASTLOCK_INIT(m->lock);
            QTHREAD_HOLD_TIMER_INIT(m);
            m->owner = me->thread_id;
            inserted = 1;
            QTHREAD_FASTLOCK_LOCK(&m->lock);
            qassertnot(qt_hash_put_locked(qlib->locks[lockbin], (void *)a, m), 0);
        } else {
            QTHREAD_FASTLOCK_LOCK(&m->lock);
        }
    }
    qt_hash_unlock(qlib->locks[lockbin]);
#endif /* ifdef LOCK_FREE_FEBS */
    assert(m);
    if (inserted) {
        QTHREAD_FASTLOCK_UNLOCK(&m->lock);
        qthread_debug(LOCK_BEHAVIOR,
                      "tid(%u), a(%p): returned (wasn't locked)\n",
                      me->thread_id, a);
    } else {
        /* someone else has it */
        QTHREAD_WAIT_TIMER_DECLARATION;

        me->thread_state     = QTHREAD_STATE_BLOCKED;
        me->rdata->blockedon = m;

        QTHREAD_WAIT_TIMER_START();

        qthread_back_to_master(me);

        QTHREAD_WAIT_TIMER_STOP(me, lockwait);

        /* once I return to this context, I own the lock! */
        /* conveniently, whoever unlocked me already set up everything too */
        qthread_debug(LOCK_BEHAVIOR,
                      "tid(%u), a(%p): returned (was locked)\n",
                      me->thread_id, a);
    }
    QTHREAD_LOCK_TIMER_STOP(aquirelock, me);
    QTHREAD_HOLD_TIMER_START(m);
    return QTHREAD_SUCCESS;
}                      /*}}} */

static inline void qthread_lock_schedule(qthread_t          *waiter,
                                         qthread_shepherd_t *shep)
{
    waiter->thread_state = QTHREAD_STATE_RUNNING;
    if (waiter->flags & QTHREAD_UNSTEALABLE) {
        qt_threadqueue_enqueue(waiter->rdata->shepherd_ptr->ready, waiter);
    } else {
#ifdef QTHREAD_USE_SPAWNCACHE
        if (!qt_spawncache_spawn(waiter))
#endif
        qt_threadqueue_enqueue(shep->ready, waiter);
    }
}

int API_FUNC qthread_unlock(const aligned_t *a)
{                      /*{{{ */
    qthread_lock_t *m;
    qthread_t      *u;
    const int       lockbin = QTHREAD_CHOOSE_STRIPE(a);
    qthread_t      *me      = qthread_internal_self();

    assert(qthread_library_initialized);

    if (!me) {
        qthread_lock_blocker_func((void *)a, UNLOCK);
        return QTHREAD_SUCCESS;
    }
    qthread_debug(LOCK_BEHAVIOR, "tid(%u), a(%p)\n", me->thread_id, a);

    QTHREAD_COUNT_THREADS_BINCOUNTER(locks, lockbin);

#ifdef LOCK_FREE_FEBS
    do {
        qthread_lock_t *m2;
        m = (qthread_lock_t *)qt_hash_get(qlib->locks[lockbin], (void *)a);
got_m:
        if (!m) { /* already unlocked */ return QTHREAD_SUCCESS; }
        hazardous_ptr(0, m);
        if (m != (m2 = qt_hash_get(qlib->locks[lockbin], (void *)a))) {
            m = m2;
            goto got_m;
        }
        if (!m->valid) { continue; }
        QTHREAD_FASTLOCK_LOCK(&m->lock);
        if (!m->valid) {
            QTHREAD_FASTLOCK_UNLOCK(&m->lock);
            continue;
        }
        break;
    } while (1);
#else
    qt_hash_lock(qlib->locks[lockbin]);
    {
        m = (qthread_lock_t *)qt_hash_get_locked(qlib->locks[lockbin], (void *)a);
        if (m == NULL) {
            /* unlocking an address that's already unlocked */
            qt_hash_unlock(qlib->locks[lockbin]);
            return QTHREAD_SUCCESS;
        }
        QTHREAD_FASTLOCK_LOCK(&m->lock);
    }
    qt_hash_unlock(qlib->locks[lockbin]);
#endif

    assert(m);

    QTHREAD_HOLD_TIMER_STOP(m, me->rdata->shepherd_ptr);

    /* unlock the address... if anybody's waiting for it, give them the lock
     * and put them in a ready queue.  If not, delete the lock structure.
     */

    QTHREAD_FASTLOCK_LOCK(&m->waiting->lock);
    u = qthread_dequeue(m->waiting);
    if (u == NULL) {
        QTHREAD_FASTLOCK_UNLOCK(&m->waiting->lock);
        QTHREAD_FASTLOCK_UNLOCK(&m->lock);

        qthread_debug(LOCK_DETAILS,
                      "tid(%u), a(%p): deleting waiting queue\n",
                      me->thread_id, a);

#ifdef LOCK_FREE_FEBS
        do {
            qthread_lock_t *m2;
            m = qt_hash_get(qlib->locks[lockbin], (void *)a);
got_m_delete:
            if (!m) { /* already gone */ break; }
            hazardous_ptr(0, m);
            if (m != (m2 = qt_hash_get(qlib->locks[lockbin], (void *)a))) {
                m = m2;
                goto got_m_delete;
            }
            if (!m->valid) { /* already gone */
                m = NULL;
                break;
            }
            QTHREAD_FASTLOCK_LOCK(&m->lock);
            if (!m->valid) { /* already gone */
                QTHREAD_FASTLOCK_UNLOCK(&m->lock);
                m = NULL;
                break;
            }
            if (m->waiting->head == NULL) {
                qassertnot(qt_hash_remove(qlib->locks[lockbin], (void *)a), 0);
                m->valid = 0;
            } else {
                QTHREAD_FASTLOCK_UNLOCK(&m->lock);
                m = NULL;
            }
            break;
        } while (1);
#else
        qt_hash_lock(qlib->locks[lockbin]);
        {
            m = (qthread_lock_t *)qt_hash_get_locked(qlib->locks[lockbin], (void *)a);
            if (m) {
                QTHREAD_FASTLOCK_LOCK(&m->lock);
                QTHREAD_FASTLOCK_LOCK(&m->waiting->lock);
                if (m->waiting->head == NULL) {
                    qassertnot(qt_hash_remove_locked(qlib->locks[lockbin], (void *)a), 0);
                } else {
                    QTHREAD_FASTLOCK_UNLOCK(&m->waiting->lock);
                    QTHREAD_FASTLOCK_UNLOCK(&m->lock);
                    m = NULL;
                }
            }
        }
        qt_hash_unlock(qlib->locks[lockbin]);
#endif

        if (m) {
            QTHREAD_HOLD_TIMER_DESTROY(m);
            QTHREAD_FASTLOCK_UNLOCK(&m->waiting->lock);
            qthread_queue_free(m->waiting);
            QTHREAD_FASTLOCK_UNLOCK(&m->lock);
            QTHREAD_FASTLOCK_DESTROY(m->lock);
            FREE_LOCK(m);
        }
    } else {
        qthread_debug(LOCK_DETAILS,
                      "tid(%u), a(%p): pulling thread from queue (%p)\n",
                      me->thread_id, a, u);
        m->owner = u->thread_id;

        qthread_lock_schedule(u, me->rdata->shepherd_ptr);

        QTHREAD_FASTLOCK_UNLOCK(&m->waiting->lock);
        QTHREAD_FASTLOCK_UNLOCK(&m->lock);
    }

    return QTHREAD_SUCCESS;
}                      /*}}} */

static QINLINE qthread_queue_t *qthread_queue_new(void)
{                      /*{{{ */
    qthread_queue_t *q;

    q = ALLOC_QUEUE();
    if (q != NULL) {
        q->head = NULL;
        q->tail = NULL;
        QTHREAD_FASTLOCK_INIT(q->lock);
    }
    return q;
}                      /*}}} */

static QINLINE qthread_t *qthread_dequeue(qthread_queue_t *q)
{                      /*{{{ */
    qthread_t *t = NULL;

    /* NOTE: it's up to the caller to lock/unlock the queue! */
    qthread_debug(LOCK_FUNCTIONS, "q(%p), t(%p): started\n", q, t);

    if (q->head == NULL) {
        qthread_debug(LOCK_DETAILS, "q(%p), t(%p): finished (nobody in list)\n", q, t);
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

    qthread_debug(LOCK_DETAILS, "q(%p), t(%p): finished\n", q, t);
    return (t);
}                      /*}}} */

static QINLINE void qthread_queue_free(qthread_queue_t *q)
{                      /*{{{ */
    assert((q->head == NULL) && (q->tail == NULL));
    QTHREAD_FASTLOCK_DESTROY(q->lock);
    FREE_QUEUE(q);
}                      /*}}} */

/* vim:set expandtab: */
