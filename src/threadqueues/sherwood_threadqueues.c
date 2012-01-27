#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <pthread.h>
#include <stdio.h>
#if defined(UNPOOLED_QUEUES) || defined(UNPOOLED)
# ifdef HAVE_MEMALIGN
#  include <malloc.h>
# endif
#endif
#include <stdlib.h>

/* Internal Headers */
#include "qthread/qthread.h"
#include "qt_visibility.h"
#include "qthread_innards.h"           /* for qlib (only used in steal_chunksize) */
#include "qt_shepherd_innards.h"
#include "qt_qthread_struct.h"
#include "qthread_asserts.h"
#include "qthread_prefetch.h"
#include "qt_threadqueues.h"
#include "qt_envariables.h"

/* Data Structures */
struct _qt_threadqueue_node {
    struct _qt_threadqueue_node *next;
    struct _qt_threadqueue_node *prev;
    qthread_t                   *value;
} /* qt_threadqueue_node_t */;

typedef struct _qt_threadqueue_node qt_threadqueue_node_t;

struct _qt_threadqueue {
    qt_threadqueue_node_t *head;
    qt_threadqueue_node_t *tail;
    /* used for the work stealing queue implementation */
    QTHREAD_TRYLOCK_TYPE  qlock;
    qthread_shepherd_t    *creator_ptr;
    long                   qlength;
    long                   qlength_stealable;                   /* number of stealable tasks on queue - stop steal attempts
                                                                 * that will fail because tasks cannot be moved - 4/1/11 AKP
                                                                 */
} /* qt_threadqueue_t */;

static long steal_chunksize = 0;

// Forward declarations
qt_threadqueue_node_t INTERNAL *qt_threadqueue_dequeue_steal(qt_threadqueue_t *h, qt_threadqueue_t *v);

void INTERNAL qt_threadqueue_enqueue_multiple(qt_threadqueue_t      *q,
                                              qt_threadqueue_node_t *first,
                                              qthread_shepherd_t    *shep);

#if defined(AKP_DEBUG) && AKP_DEBUG
/* function added to ease debugging and tuning around queue critical sections - 4/1/11 AKP */

void qt_spin_exclusive_lock(qt_spin_exclusive_t *l)
{
    uint64_t val = qthread_incr(&l->enter, 1);

    while (val != l->exit) {} // spin waiting for my turn
}

void qt_spin_exclusive_unlock(qt_spin_exclusive_t *l)
{
    qthread_incr(&l->exit, 1); // allow next guy's turn
}

/* end of added functions - AKP */
#endif /* if AKP_DEBUG */

/* Memory Management */
#if defined(UNPOOLED_QUEUES) || defined(UNPOOLED)
# define ALLOC_THREADQUEUE() (qt_threadqueue_t *)calloc(1, sizeof(qt_threadqueue_t))
# define FREE_THREADQUEUE(t) free(t)
static QINLINE void ALLOC_TQNODE(qt_threadqueue_node_t **ret)
{                                      /*{{{ */
# ifdef HAVE_MEMALIGN
    *ret = (qt_threadqueue_node_t *)memalign(16, sizeof(qt_threadqueue_node_t));
# elif defined(HAVE_POSIX_MEMALIGN)
    qassert(posix_memalign((void **)ret, 16, sizeof(qt_threadqueue_node_t)),
            0);
# else
    *ret = calloc(1, sizeof(qt_threadqueue_node_t));
    return;
# endif
    memset(*ret, 0, sizeof(qt_threadqueue_node_t));
}                                      /*}}} */

# define FREE_TQNODE(t) free(t)
void INTERNAL qt_threadqueue_subsystem_init(void) {}
#else /* if defined(UNPOOLED_QUEUES) || defined(UNPOOLED) */
qt_threadqueue_pools_t generic_threadqueue_pools;
# define ALLOC_THREADQUEUE() (qt_threadqueue_t *)qt_mpool_cached_alloc(generic_threadqueue_pools.queues)
# define FREE_THREADQUEUE(t) qt_mpool_cached_free(generic_threadqueue_pools.queues, t)

static QINLINE void ALLOC_TQNODE(qt_threadqueue_node_t **ret)
{                                      /*{{{ */
    *ret = (qt_threadqueue_node_t *)qt_mpool_cached_alloc(generic_threadqueue_pools.nodes);
    if (*ret != NULL) {
        memset(*ret, 0, sizeof(qt_threadqueue_node_t));
    }
}                                      /*}}} */

# define FREE_TQNODE(t) qt_mpool_cached_free(generic_threadqueue_pools.nodes, t)

static void qt_threadqueue_subsystem_shutdown(void)
{
    qt_mpool_destroy(generic_threadqueue_pools.nodes);
    qt_mpool_destroy(generic_threadqueue_pools.queues);
}

void INTERNAL qt_threadqueue_subsystem_init(void)
{
    steal_chunksize = qt_internal_get_env_num("STEAL_CHUNKSIZE", qlib->nworkerspershep, 1);
    generic_threadqueue_pools.nodes  = qt_mpool_create_aligned(sizeof(qt_threadqueue_node_t), 16);
    generic_threadqueue_pools.queues = qt_mpool_create(sizeof(qt_threadqueue_t));
    qthread_internal_cleanup(qt_threadqueue_subsystem_shutdown);
}

void INTERNAL qt_threadqueue_init_pools(qt_threadqueue_pools_t *p)
{   /*{{{*/
    p->nodes  = qt_mpool_create_aligned(sizeof(qt_threadqueue_node_t), 16);
    p->queues = qt_mpool_create(sizeof(qt_threadqueue_t));
} /*}}}*/

void INTERNAL qt_threadqueue_destroy_pools(qt_threadqueue_pools_t *p)
{   /*{{{*/
    qt_mpool_destroy(p->nodes);
    qt_mpool_destroy(p->queues);
} /*}}}*/

#endif /* if defined(UNPOOLED_QUEUES) || defined(UNPOOLED) */

ssize_t INTERNAL qt_threadqueue_advisory_queuelen(qt_threadqueue_t *q)
{   /*{{{*/
#if ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) ||    \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64) ||      \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64) || \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64))
    /* only works if a basic load is atomic */
    return q->qlength;

#else
    ssize_t tmp;
    QTHREAD_TRYLOCK_LOCK(&q->qlock);
    tmp = q->qlength;
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
    return tmp;
#endif /* if ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)) */
} /*}}}*/

#define QTHREAD_INITLOCK(l) do { if (pthread_mutex_init(l, NULL) != 0) { return QTHREAD_PTHREAD_ERROR; } } while(0)
#define QTHREAD_LOCK(l)     qassert(pthread_mutex_lock(l), 0)
#define QTHREAD_UNLOCK(l)   qassert(pthread_mutex_unlock(l), 0)
// #define QTHREAD_DESTROYLOCK(l) do { int __ret__ = pthread_mutex_destroy(l); if (__ret__ != 0) fprintf(stderr, "pthread_mutex_destroy(%p) returned %i (%s)\n", l, __ret__, strerror(__ret__)); assert(__ret__ == 0); } while (0)
#define QTHREAD_DESTROYLOCK(l) qassert(pthread_mutex_destroy(l), 0)
#define QTHREAD_DESTROYCOND(l) qassert(pthread_cond_destroy(l), 0)
#define QTHREAD_SIGNAL(l)      qassert(pthread_cond_signal(l), 0)
#define QTHREAD_CONDWAIT(c, l) qassert(pthread_cond_wait(c, l), 0)

/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

static QINLINE void qthread_steal(void);

qt_threadqueue_t INTERNAL *qt_threadqueue_new(qthread_shepherd_t *shepherd)
{   /*{{{*/
    qt_threadqueue_t *q = ALLOC_THREADQUEUE();

    if (q != NULL) {
        q->head              = q->tail = NULL;
        q->qlength           = 0;
        q->qlength_stealable = 0;
        QTHREAD_TRYLOCK_INIT(q->qlock);
    }
    q->creator_ptr = shepherd;

    return q;
} /*}}}*/

void INTERNAL qt_threadqueue_free(qt_threadqueue_t *q)
{   /*{{{*/
    while (q->head != q->tail) {
        qt_threadqueue_dequeue_blocking(q, 1);
    }
    assert(q->head == q->tail);
    QTHREAD_TRYLOCK_DESTROY(q->qlock);
    FREE_THREADQUEUE(q);
} /*}}}*/

/* enqueue at tail */
void INTERNAL qt_threadqueue_enqueue(qt_threadqueue_t   *q,
                                     qthread_t          *t,
                                     qthread_shepherd_t *shep)
{   /*{{{*/
    qt_threadqueue_node_t *node;

    ALLOC_TQNODE(&node);
    assert(node != NULL);

    node->value = t;

    assert(q != NULL);
    assert(t != NULL);

#if 0
#else
    QTHREAD_TRYLOCK_LOCK(&q->qlock);
    node->next = NULL;
    node->prev = q->tail;
    q->tail    = node;
    if (q->head == NULL) {
        q->head = node;
    } else {
        node->prev->next = node;
    }
    q->qlength++;
    if (!(t->flags & QTHREAD_UNSTEALABLE)) { q->qlength_stealable++; }
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
#endif /* if 0 */
} /*}}}*/

/* yielded threads enqueue at head */
void INTERNAL qt_threadqueue_enqueue_yielded(qt_threadqueue_t   *q,
                                             qthread_t          *t,
                                             qthread_shepherd_t *shep)
{   /*{{{*/
    qt_threadqueue_node_t *node;

    ALLOC_TQNODE(&node);
    assert(node != NULL);

    node->value = t;

    assert(q != NULL);
    assert(t != NULL);

    QTHREAD_TRYLOCK_LOCK(&q->qlock);
    node->prev = NULL;
    node->next = q->head;
    q->head    = node;
    if (q->tail == NULL) {
        q->tail = node;
    } else {
        node->next->prev = node;
    }
    q->qlength++;
    if (!(t->flags & QTHREAD_UNSTEALABLE)) { q->qlength_stealable++; }
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
} /*}}}*/

/* dequeue at tail, unlike original qthreads implementation */
qthread_t INTERNAL *qt_threadqueue_dequeue_blocking(qt_threadqueue_t *q,
                                                    size_t            active)
{   /*{{{*/
    qt_threadqueue_node_t *node;
    qthread_t             *t;

    assert(q != NULL);

    while (1) {
        QTHREAD_TRYLOCK_LOCK(&q->qlock);
        node = q->tail;
        if (node != NULL) {
            q->tail = q->tail->prev;
            if (q->tail == NULL) {
                q->head = NULL;
            } else {
                q->tail->next = NULL;
            }
            q->qlength--;
            if (!(node->value->flags & QTHREAD_UNSTEALABLE)) { q->qlength_stealable--; }
        }
        QTHREAD_TRYLOCK_UNLOCK(&q->qlock);

        if ((node == NULL) && (active)) {
            qthread_steal();
        } else {
            if (node) {                // watch out for inactive node not stealling
                t = node->value;
                FREE_TQNODE(node);
                if ((t->flags & QTHREAD_MUST_BE_WORKER_ZERO)) { // only needs to be on worker 0 for termination
                    switch(qthread_worker(NULL)) {
                        case NO_WORKER:
                            QTHREAD_TRAP(); // should never happen
                            t = NULL;
                            continue; // keep looking
                        case 0:
                            return(t);

                        default:
                            /* McCoy thread can only run on worker 0 */
                            qt_threadqueue_enqueue_yielded(q, t, (t->rdata) ? (t->rdata->shepherd_ptr) : NULL);
                            t = NULL;
                            continue; // keep looking
                    }
                } else {
                    break;
                }
            }
        }
    }
    return (t);
} /*}}}*/

/* enqueue multiple (from steal) */
void INTERNAL qt_threadqueue_enqueue_multiple(qt_threadqueue_t      *q,
                                              qt_threadqueue_node_t *first,
                                              qthread_shepherd_t    *shep)
{   /*{{{*/
    qt_threadqueue_node_t *last;
    size_t                 addCnt = 1;

    assert(first != NULL);
    assert(q != NULL);

    last                         = first;
    last->value->target_shepherd = shep;        // Defeats default of "sending home" to original shepherd
    while (last->next) {
        last                         = last->next;
        last->value->target_shepherd = shep;    // Defeats default of "sending home" to original shepherd
        addCnt++;
    }

    QTHREAD_TRYLOCK_LOCK(&q->qlock);
    last->next  = NULL;
    first->prev = q->tail;
    q->tail     = last;
    if (q->head == NULL) {
        q->head = first;
    } else {
        first->prev->next = first;
    }
    q->qlength           += addCnt;
    q->qlength_stealable += addCnt;
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
} /*}}}*/

/* dequeue stolen threads at head, skip yielded threads */
qt_threadqueue_node_t INTERNAL *qt_threadqueue_dequeue_steal(qt_threadqueue_t *h, qt_threadqueue_t *v)
{                                      /*{{{ */
    qt_threadqueue_node_t *node;
    qt_threadqueue_node_t *first     = NULL;
    qt_threadqueue_node_t *last      = NULL;
    long                   amtStolen = 0;
    long                   desired_stolen = v->qlength_stealable / 2;

    assert(q != NULL);

    if (desired_stolen == 0) desired_stolen = 1;

    if (!QTHREAD_TRYLOCK_TRY(&v->qlock)) {
        return NULL;
    }
    while (v->qlength_stealable > 0 && amtStolen < desired_stolen) {
        node = (qt_threadqueue_node_t *)v->head;
        while (node != NULL &&
               (node->value->thread_state == QTHREAD_STATE_YIELDED ||
                node->value->thread_state == QTHREAD_STATE_TERM_SHEP ||
                (node->value->flags & QTHREAD_UNSTEALABLE))
               ) {
            node = (qt_threadqueue_node_t *)node->next;
        }
        if (node != NULL) {
            if (node == v->head) {
                v->head = node->next;
            } else {
                node->prev->next = node->next;
            }
            if (node == v->tail) {
                v->tail = node->prev;
            } else {
                node->next->prev = node->prev;
            }

            v->qlength--;
            v->qlength_stealable--;

            node->prev = node->next = NULL;
            if (first == NULL) {
                first = last = node;
            } else {
                last->next = node;
                node->prev = last;
                last       = node;
            }
            amtStolen++;
        } else {
            break;
        }
    }
    QTHREAD_TRYLOCK_UNLOCK(&v->qlock);
#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
    qthread_incr(&v->creator_ptr->steal_amount_stolen, amtStolen);
#endif

    return (first);
}                                      /*}}} */

/*  Steal work from another shepherd's queue
 *    Returns the amount of work stolen
 */
static QINLINE void qthread_steal(void)
{   /*{{{*/
    unsigned int           i;
    extern pthread_key_t   shepherd_structs;
    qt_threadqueue_node_t *first;
    qthread_shepherd_t    *victim_shepherd;
    qthread_worker_t      *worker = qthread_internal_getworker();
    assert(worker != NULL);
    qthread_shepherd_t *thief_shepherd = worker->shepherd;

#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
    qthread_incr(&thief_shepherd->steal_called, 1);
#endif
    if (thief_shepherd->stealing) {
        // this means that someone else on this shepherd is already stealing; I will spin on my own queue.
        return;
    } else {
#ifdef QTHREAD_OMP_AFFINITY/*{{{*/
        if (thief_shepherd->stealing_mode == QTHREAD_STEAL_ON_ALL_IDLE) {
            for (i = 0; i < qlib->nworkerspershep; i++)
                if (thief_shepherd->workers[i].current != NULL) {
                    return;
                }
            thief_shepherd->stealing_mode = QTHREAD_STEAL_ON_ANY_IDLE;
        }
#endif/*}}}*/
        if (qthread_cas(&thief_shepherd->stealing, 0, 1) != 0) { // avoid unnecessary stealing with a CAS
            return;
        }
    }
#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
    qthread_incr(&thief_shepherd->steal_attempted, 1);
#endif
    for (i = 1; i < qlib->nshepherds; i++) {
        victim_shepherd = &qlib->shepherds[thief_shepherd->sorted_sheplist[i - 1]];
        if (0 < victim_shepherd->ready->qlength_stealable) {
            first = qt_threadqueue_dequeue_steal(thief_shepherd->ready, victim_shepherd->ready);
            if (first) {
                qt_threadqueue_enqueue_multiple(thief_shepherd->ready, first,
                                                thief_shepherd);
#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
                qthread_incr(&thief_shepherd->steal_successful, 1);
#endif
                break;
            }
#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
            else {
                qthread_incr(&thief_shepherd->steal_failed, 1);
            }
#endif
        }
        if (0 < thief_shepherd->ready->qlength) {  // work at home quit steal attempt
            break;
        }
    }
    thief_shepherd->stealing = 0;
} /*}}}*/

#ifdef STEAL_PROFILE                   // should give mechanism to make steal profiling optional
void INTERNAL qthread_steal_stat(void)
{
    int i;

    assert(qlib);
    for (i = 0; i < qlib->nshepherds; i++) {
        fprintf(stdout,
                "shepherd %d - steals called %ld attempted %ld failed %ld successful %ld work stolen %ld\n",
                qlib->shepherds[i].shepherd_id,
                qlib->shepherds[i].steal_called,
                qlib->shepherds[i].steal_attempted,
                qlib->shepherds[i].steal_failed,
                qlib->shepherds[i].steal_successful,
                qlib->shepherds[i].steal_amount_stolen);
    }
}

#endif  /* ifdef STEAL_PROFILE */

/* walk queue looking for a specific value  -- if found remove it (and start
 * it running)  -- if not return NULL
 */
qthread_t INTERNAL *qt_threadqueue_dequeue_specific(qt_threadqueue_t *q,
                                                    void             *value)
{
    qt_threadqueue_node_t *node = NULL;
    qthread_t             *t    = NULL;

    assert(q != NULL);

    QTHREAD_TRYLOCK_LOCK(&q->qlock);
    if (q->qlength > 0) {
        node = (qt_threadqueue_node_t *)q->tail;
        t    = (node) ? (qthread_t *)node->value : NULL;
        while ((t != NULL) && (t->ret != value)) {
            node = (qt_threadqueue_node_t *)node->prev;
            t    = (node) ? (qthread_t *)node->value : NULL;
        }
        if ((node != NULL)) {
            if (node != q->tail) {
                if (node == q->head) {
                    q->head = node->next;       // reset front ptr
                } else {
                    node->prev->next = node->next;
                }
                node->next->prev = node->prev;  // reset back ptr (know we're not tail
                node->next       = NULL;
                node->prev       = q->tail;
                q->tail->next    = node;
                q->tail          = node;
            }
        }
    }
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);

    return (t);
}

/* vim:set expandtab: */
