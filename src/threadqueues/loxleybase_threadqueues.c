#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <pthread.h>
#include <stdint.h>
#include <emmintrin.h>
#include <stdio.h>

/* Internal Headers */
#include "qthread/qthread.h"
#include "qt_visibility.h"
#include "qthread_innards.h"           /* for qlib (only used in steal_chunksize) */
#include "qt_shepherd_innards.h"
#include "qt_qthread_struct.h"
#include "qt_threadqueues.h"
#include "qt_envariables.h"
#include "qt_threadqueue_stack.h"

#ifdef STEAL_PROFILE
# define steal_profile_increment(shepherd, field) qthread_incr(&(shepherd->field), 1)
#else
# define steal_profile_increment(x, y) do { } while(0)
#endif

struct _qt_threadqueue {
    QTHREAD_FASTLOCK_TYPE spinlock;
    QTHREAD_FASTLOCK_TYPE steallock;

    qt_stack_t            stack;

    /* used for the work stealing queue implementation */
    uint32_t empty;
    uint32_t stealing;
} /* qt_threadqueue_t */;

// Forward declarations

void INTERNAL qt_threadqueue_enqueue_multiple(qt_threadqueue_t   *q,
                                              int                 stealcount,
                                              qthread_t         **stealbuffer,
                                              qthread_shepherd_t *shep);

void INTERNAL qt_threadqueue_subsystem_init(void) {}

/*****************************************/
/* functions to manage the thread queues */
/*****************************************/

static QINLINE long       qthread_steal_chunksize(void);
static QINLINE qthread_t *qthread_steal(qt_threadqueue_t *thiefq);

qt_threadqueue_t INTERNAL *qt_threadqueue_new(void)
{   /*{{{*/
    qt_threadqueue_t *q;

    posix_memalign((void **)&q, 64, sizeof(qt_threadqueue_t));

    if (q != NULL) {
        q->empty    = 1;
        q->stealing = 0;
        qt_stack_create(&(q->stack), 1024);
        QTHREAD_FASTLOCK_INIT(q->spinlock);
        QTHREAD_FASTLOCK_INIT(q->steallock);
    }
    return q;
}   /*}}}*/

void INTERNAL qt_threadqueue_free(qt_threadqueue_t *q)
{   /*{{{*/
    qt_stack_free(&q->stack);
    free((void *)q);
} /*}}}*/

ssize_t INTERNAL qt_threadqueue_advisory_queuelen(qt_threadqueue_t *q)
{   /*{{{*/
    ssize_t retval;

    QTHREAD_FASTLOCK_LOCK(&q->spinlock);
    retval = qt_stack_size(&q->stack);
    QTHREAD_FASTLOCK_UNLOCK(&q->spinlock);
    return 0;
} /*}}}*/

#ifdef QTHREAD_USE_SPAWNCACHE
int INTERNAL qt_threadqueue_private_enqueue(qt_threadqueue_private_t *restrict q,
                                            qthread_t *restrict                t)
{
    return 0;
}

int INTERNAL qt_threadqueue_private_enqueue_yielded(qt_threadqueue_private_t *restrict q,
                                                    qthread_t *restrict                t)
{
    return 0;
}

#endif /* ifdef QTHREAD_USE_SPAWNCACHE */

/* enqueue at tail */
void INTERNAL qt_threadqueue_enqueue(qt_threadqueue_t *restrict q,
                                     qthread_t *restrict        t)
{   /*{{{*/
    QTHREAD_FASTLOCK_LOCK(&q->spinlock);
    qt_stack_push(&q->stack, t);
    QTHREAD_FASTLOCK_UNLOCK(&q->spinlock);
    q->empty = 0;
} /*}}}*/

/* enqueue multiple (from steal) */
void INTERNAL qt_threadqueue_enqueue_multiple(qt_threadqueue_t   *q,
                                              int                 stealcount,
                                              qthread_t         **stealbuffer,
                                              qthread_shepherd_t *shep)
{   /*{{{*/
    /* save element 0 for the thief */
    QTHREAD_FASTLOCK_LOCK(&q->spinlock);
    for(int i = 1; i < stealcount; i++) {
        qthread_t *t = stealbuffer[i];
        t->target_shepherd = shep;
        qt_stack_push(&q->stack, t);
    }
    QTHREAD_FASTLOCK_UNLOCK(&q->spinlock);
    q->empty = 0;
} /*}}}*/

/* yielded threads enqueue at head */
void INTERNAL qt_threadqueue_enqueue_yielded(qt_threadqueue_t *restrict q,
                                             qthread_t *restrict        t)
{   /*{{{*/
    QTHREAD_FASTLOCK_LOCK(&q->spinlock);
    qt_stack_enq_base(&q->stack, t);
    QTHREAD_FASTLOCK_UNLOCK(&q->spinlock);
    q->empty = 0;
} /*}}}*/

qthread_t static QINLINE *qt_threadqueue_dequeue_helper(qt_threadqueue_t *q)
{
    qthread_t *t = NULL;

    q->stealing = 1;

    QTHREAD_FASTLOCK_LOCK(&q->steallock);
    if (q->stealing) {
        t = qthread_steal(q);
    }
    QTHREAD_FASTLOCK_UNLOCK(&q->steallock);

    return(t);
}

/* dequeue at tail, unlike original qthreads implementation */
qthread_t INTERNAL *qt_threadqueue_dequeue_blocking(qt_threadqueue_t         *q,
                                                    qt_threadqueue_private_t *QUNUSED(qc),
                                                    uint_fast8_t              active)
{   /*{{{*/
    qthread_t *t;

    for(;;) {
        QTHREAD_FASTLOCK_LOCK(&q->spinlock);
        t = qt_stack_pop(&q->stack);
        QTHREAD_FASTLOCK_UNLOCK(&q->spinlock);
        if (t != NULL) { return(t); }
        t = qt_threadqueue_dequeue_helper(q);
        if (t != NULL) { return(t); }
    }
}   /*}}}*/

int static QINLINE qt_threadqueue_stealable(qthread_t *t)
{
    return(t->thread_state != QTHREAD_STATE_YIELDED &&
           t->thread_state != QTHREAD_STATE_TERM_SHEP &&
           !(t->flags & QTHREAD_UNSTEALABLE));
}

/* Returns the number of tasks to steal per steal operation (chunk size) */
static QINLINE long qthread_steal_chunksize(void)
{   /*{{{*/
    static long chunksize = 0;

    if (chunksize == 0) {
        chunksize = qt_internal_get_env_num("STEAL_CHUNKSIZE", qlib->nworkerspershep, 1);
    }

    return chunksize;
}   /*}}}*/

static void qt_threadqueue_enqueue_unstealable(qt_stack_t *stack,
                                               qthread_t **nostealbuffer,
                                               int         amtNotStolen,
                                               int         index)
{
    int         capacity = stack->capacity;
    qthread_t **storage  = stack->storage;
    int         i;

    for(i = amtNotStolen - 1; i >= 0; i--) {
        storage[index] = nostealbuffer[i];
        index          = (index - 1 + capacity) % capacity;
    }
    if (stack->top == index) { stack->empty = 1; }

    storage[index] = NULL;
    stack->base    = index;
}

static int qt_threadqueue_steal(qt_threadqueue_t *victim_queue,
                                qthread_t       **nostealbuffer,
                                qthread_t       **stealbuffer)
{
    qthread_t  *candidate;
    qt_stack_t *stack = &victim_queue->stack;

    if (qt_stack_is_empty(stack)) {
        victim_queue->empty = 1;
        return(0);
    }

    int         amtStolen = 0, amtNotStolen = 0;
    int         maxStolen = qthread_steal_chunksize();
    int         base      = stack->base;
    int         top       = stack->top;
    int         capacity  = stack->capacity;
    qthread_t **storage   = stack->storage;

    int index = base;

    while (amtStolen < maxStolen) {
        index     = (index + 1) % capacity;
        candidate = storage[index];
        if (qt_threadqueue_stealable(candidate)) {
            stealbuffer[amtStolen++] = candidate;
        } else {
            nostealbuffer[amtNotStolen++] = candidate;
        }
        storage[index] = NULL;
        if (index == top) {
            victim_queue->empty = 1;
            break;
        }
    }

    qt_threadqueue_enqueue_unstealable(stack, nostealbuffer, amtNotStolen, index);

#ifdef STEAL_PROFILE    // should give mechanism to make steal profiling optional
    qthread_incr(&victim_queue->steal_amount_stolen, amtStolen);
#endif

    return(amtStolen);
}

/*  Steal work from another shepherd's queue
 *    Returns the amount of work stolen
 */
static QINLINE qthread_t *qthread_steal(qt_threadqueue_t *thiefq)
{   /*{{{*/
    int i, amtStolen;

    extern TLS_DECL(qthread_shepherd_t *, shepherd_structs);
    qthread_shepherd_t *victim_shepherd;
    qt_threadqueue_t   *victim_queue;
    qthread_worker_t   *worker =
        (qthread_worker_t *)TLS_GET(shepherd_structs);
    qthread_shepherd_t *thief_shepherd =
        (qthread_shepherd_t *)worker->shepherd;
    qthread_t **nostealbuffer = worker->nostealbuffer;
    qthread_t **stealbuffer   = worker->stealbuffer;
    int         nshepherds    = qlib->nshepherds;

    steal_profile_increment(thief_shepherd, steal_called);

#ifdef QTHREAD_OMP_AFFINITY
    if (thief_shepherd->stealing_mode == QTHREAD_STEAL_ON_ALL_IDLE) {
        for (i = 0; i < qlib->nworkerspershep; i++)
            if (thief_shepherd->workers[i].current != NULL) {
                thiefq->stealing = 0;
                return(NULL);
            }
        thief_shepherd->stealing_mode = QTHREAD_STEAL_ON_ANY_IDLE;
    }
#endif

    steal_profile_increment(thief_shepherd, steal_attempted);

    int shepherd_offset = qthread_worker(NULL) % nshepherds;

    for (i = 1; i < qlib->nshepherds; i++) {
        shepherd_offset = (shepherd_offset + 1) % nshepherds;
        if (shepherd_offset == thief_shepherd->shepherd_id) {
            shepherd_offset = (shepherd_offset + 1) % nshepherds;
        }
        victim_shepherd = &qlib->shepherds[shepherd_offset];
        victim_queue    = victim_shepherd->ready;
        if (victim_queue->empty) { continue; }

        QTHREAD_FASTLOCK_LOCK(&victim_queue->spinlock);
        amtStolen = qt_threadqueue_steal(victim_queue, nostealbuffer, stealbuffer);
        QTHREAD_FASTLOCK_UNLOCK(&victim_queue->spinlock);

        if (amtStolen > 0) {
            qt_threadqueue_enqueue_multiple(thiefq, amtStolen,
                                            stealbuffer, thief_shepherd);
            thiefq->stealing = 0;
            steal_profile_increment(thief_shepherd, steal_successful);
            return(stealbuffer[0]);
        } else {
            steal_profile_increment(thief_shepherd, steal_failed);
        }
    }
    thiefq->stealing = 0;
    return(NULL);
}   /*}}}*/

/* walk queue looking for a specific value  -- if found remove it (and start
 * it running)  -- if not return NULL
 */
qthread_t INTERNAL *qt_threadqueue_dequeue_specific(qt_threadqueue_t *q,
                                                    void             *value)
{   /*{{{*/
    return (NULL);
}   /*}}}*/

#ifdef STEAL_PROFILE  // should give mechanism to make steal profiling optional
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

/* vim:set expandtab: */
