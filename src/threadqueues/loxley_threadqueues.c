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
#include "qt_macros.h"
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

struct qt_threadqueue_local_s {
    QTHREAD_FASTLOCK_TYPE lock;
    qt_stack_t            stack;
    unsigned int          bias;
    unsigned int          penalty;
    unsigned int          track;
};

typedef struct qt_threadqueue_local_s qt_threadqueue_local_t;

struct _qt_threadqueue {
    qt_threadqueue_local_t **local;

    uint32_t                 padding[(CACHELINE_WIDTH - sizeof(qt_threadqueue_local_t * *)) / sizeof(uint32_t)];

    QTHREAD_TRYLOCK_TYPE     trylock;
    QTHREAD_FASTLOCK_TYPE    steallock;

    qthread_shepherd_t      *creator_ptr;
    qt_stack_t               shared_stack;

    /* used for the work stealing queue implementation */
    uint32_t empty;
    uint32_t stealing;
} /* qt_threadqueue_t */;

struct _qt_threadqueue_private {} /* qt_threadqueue_private_t */;

static const int enqueue_penalty_max   = 1048576;
static const int enqueue_penalty_min   = 1;
static const int enqueue_penalty_limit = 20;

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
static QINLINE long       qthread_bias_penalty(void);
static QINLINE qthread_t *qthread_steal(qt_threadqueue_t *thiefq);

qt_threadqueue_t INTERNAL *qt_threadqueue_new(qthread_shepherd_t *shepherd)
{   /*{{{*/
    int               i;
    int               local_length = qlib->nworkerspershep + 1;
    qt_threadqueue_t *q;

    posix_memalign((void **)&q, 64, sizeof(qt_threadqueue_t));

    if (q != NULL) {
        q->creator_ptr = shepherd;
        q->empty       = 1;
        q->stealing    = 0;
        qt_stack_create(&(q->shared_stack), 1024);
        QTHREAD_TRYLOCK_INIT(q->trylock);
        QTHREAD_FASTLOCK_INIT(q->steallock);
        q->local = calloc(local_length, sizeof(qt_threadqueue_local_t *));
        for(i = 0; i < local_length; i++) {
            posix_memalign((void **)&q->local[i], 64, sizeof(qt_threadqueue_local_t));
            qt_stack_create(&(q->local[i]->stack), 1024);
            QTHREAD_FASTLOCK_INIT(q->local[i]->lock);
            q->local[i]->bias    = 0;
            q->local[i]->penalty = enqueue_penalty_min;
            q->local[i]->track   = 0;
        }
    }
    return q;
}   /*}}}*/

void INTERNAL qt_threadqueue_free(qt_threadqueue_t *q)
{   /*{{{*/
    int i;
    int local_length = qlib->nworkerspershep + 1;

    qt_stack_free(&q->shared_stack);
    for(i = 0; i < local_length; i++) {
        qt_stack_free(&(q->local[i]->stack));
        free(q->local[i]);
    }
    free(q->local);
    free(q);
} /*}}}*/

ssize_t INTERNAL qt_threadqueue_advisory_queuelen(qt_threadqueue_t *q)
{   /*{{{*/
    ssize_t retval;

    QTHREAD_TRYLOCK_LOCK(&q->trylock);
    retval = qt_stack_size(&q->shared_stack);
    QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
    return 0;
} /*}}}*/

static QINLINE qthread_worker_id_t qt_threadqueue_worker_id(void)
{
    extern pthread_key_t shepherd_structs;
    qthread_worker_id_t  id;
    qthread_worker_t    *worker = (qthread_worker_t *)
                                  pthread_getspecific(shepherd_structs);

    if (worker == NULL) { return(qlib->nworkerspershep); }
    id = worker->worker_id;
    assert(id >= 0 && id < qlib->nworkerspershep);
    return(id);
}

qt_threadqueue_private_t INTERNAL *qt_threadqueue_private_create(void)
{   /*{{{*/
    return NULL;
} /*}}}*/

void INTERNAL qt_threadqueue_private_enqueue(qt_threadqueue_private_t *restrict q,
                                             qthread_t *restrict                t)
{}

/* enqueue at tail */
void INTERNAL qt_threadqueue_enqueue(qt_threadqueue_t *restrict q,
                                     qthread_t *restrict        t)
{   /*{{{*/
    int                     id    = qt_threadqueue_worker_id();
    qt_threadqueue_local_t *local = q->local[id];

    if (local->bias) {
        QTHREAD_FASTLOCK_LOCK(&local->lock);
        qt_stack_push(&local->stack, t);
        QTHREAD_FASTLOCK_UNLOCK(&local->lock);
        (local->bias)--;
    } else {
        if(QTHREAD_TRYLOCK_TRY(&q->trylock)) {
            qt_stack_push(&q->shared_stack, t);
            QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
            if(++(local->track) > enqueue_penalty_limit) {
                if (local->penalty > enqueue_penalty_min) {
                    local->penalty /= 2;
                }
                local->track = 0;
            }
        } else {
            QTHREAD_FASTLOCK_LOCK(&local->lock);
            qt_stack_push(&local->stack, t);
            QTHREAD_FASTLOCK_UNLOCK(&local->lock);
            if(--(local->track) < -enqueue_penalty_limit) {
                if (local->penalty < enqueue_penalty_max) {
                    local->penalty *= 2;
                }
                local->track = 0;
            }
            local->bias = local->penalty;
        }
    }

    q->empty = 0;
} /*}}}*/

/* enqueue multiple (from steal) */
void INTERNAL qt_threadqueue_enqueue_multiple(qt_threadqueue_t   *q,
                                              int                 stealcount,
                                              qthread_t         **stealbuffer,
                                              qthread_shepherd_t *shep)
{   /*{{{*/
    /* save element 0 for the thief */
    QTHREAD_TRYLOCK_LOCK(&q->trylock);
    for(int i = 1; i < stealcount; i++) {
        qthread_t *t = stealbuffer[i];
        t->target_shepherd = shep;
        qt_stack_push(&q->shared_stack, t);
    }
    QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
    q->empty = 0;
} /*}}}*/

/* yielded threads enqueue at head */
void INTERNAL qt_threadqueue_enqueue_yielded(qt_threadqueue_t *restrict q,
                                             qthread_t *restrict        t)
{   /*{{{*/
    QTHREAD_TRYLOCK_LOCK(&q->trylock);
    qt_stack_enq_base(&q->shared_stack, t);
    QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
    q->empty = 0;
} /*}}}*/

qthread_t static QINLINE *qt_threadqueue_dequeue_helper(qt_threadqueue_t *q)
{
    int        i, next, id = qt_threadqueue_worker_id();
    int        local_length = qlib->nworkerspershep + 1;
    qthread_t *t            = NULL;

    for(i = 1; i < local_length; i++) {
        next = (id + i) % local_length;
        if (!q->local[next]->stack.empty) {
            QTHREAD_FASTLOCK_LOCK(&q->local[next]->lock);
            t = qt_stack_pop(&q->local[next]->stack);
            QTHREAD_FASTLOCK_UNLOCK(&q->local[next]->lock);
            if (t != NULL) { return(t); }
        }
    }

    q->stealing = 1;

    QTHREAD_FASTLOCK_LOCK(&q->steallock);

    if (q->stealing) {
        t = qthread_steal(q);
    }
    QTHREAD_FASTLOCK_UNLOCK(&q->steallock);

    return(t);
}

/* dequeue at tail, unlike original qthreads implementation */
qthread_t INTERNAL *qt_threadqueue_dequeue_blocking(qt_threadqueue_t *q,
                                                    qt_threadqueue_private_t *QUNUSED(qc),
                                                    uint_fast8_t      active)
{   /*{{{*/
    int                     id    = qt_threadqueue_worker_id();
    qt_threadqueue_local_t *local = q->local[id];
    qthread_t              *t     = NULL, *retainer;

    for(;;) {
        if (!local->stack.empty) {
            QTHREAD_FASTLOCK_LOCK(&local->lock);
            t = qt_stack_pop(&local->stack);
            QTHREAD_FASTLOCK_UNLOCK(&local->lock);
            if (t != NULL) { return(t); }
        }
        if (QTHREAD_TRYLOCK_TRY(&q->trylock)) {
            t = qt_stack_pop(&q->shared_stack);
            QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
        } else {
            QTHREAD_TRYLOCK_LOCK(&q->trylock);
            t        = qt_stack_pop(&q->shared_stack);
            retainer = qt_stack_pop(&q->shared_stack);
            QTHREAD_TRYLOCK_UNLOCK(&q->trylock);
            if (retainer != NULL) {
                QTHREAD_FASTLOCK_LOCK(&local->lock);
                qt_stack_push(&local->stack, retainer);
                QTHREAD_FASTLOCK_UNLOCK(&local->lock);
            }
        }
        if (t != NULL) { return(t); }
        t = qt_threadqueue_dequeue_helper(q);
        if (t != NULL) { return(t); }
    }
}   /*}}}*/

int static QINLINE qt_threadqueue_stealable(qthread_t *t)
{
    return(t->thread_state != QTHREAD_STATE_YIELDED &&
           t->thread_state != QTHREAD_STATE_TERM_SHEP &&
           !(t->flags & QTHREAD_MUST_BE_WORKER_ZERO) &&
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

/* Returns the number of tasks to steal per steal operation (chunk size) */
static QINLINE long qthread_bias_penalty(void)
{   /*{{{*/
    static long penalty = 0;

    if (penalty == 0) {
        penalty = qt_internal_get_env_num("BIAS_PENALTY", qlib->nworkerspershep, 1);
    }

    return penalty;
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

static int qt_threadqueue_steal_helper(qt_stack_t *stack,
                                       qthread_t **nostealbuffer,
                                       qthread_t **stealbuffer,
                                       int         amtStolen)
{
    if (qt_stack_is_empty(stack)) {
        return(amtStolen);
    }

    int         amtNotStolen = 0;
    int         maxStolen    = qthread_steal_chunksize();
    int         base         = stack->base;
    int         top          = stack->top;
    int         capacity     = stack->capacity;
    qthread_t **storage      = stack->storage;
    qthread_t  *candidate;

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
            break;
        }
    }

    qt_threadqueue_enqueue_unstealable(stack, nostealbuffer, amtNotStolen, index);

    return(amtStolen);
}

static int qt_threadqueue_steal(qt_threadqueue_t *victim_queue,
                                qthread_t       **nostealbuffer,
                                qthread_t       **stealbuffer)
{
    qt_stack_t *shared_stack = &victim_queue->shared_stack;
    int         i, amtStolen = 0;
    int         maxStolen    = qthread_steal_chunksize();
    int         local_length = qlib->nworkerspershep + 1;

    amtStolen = qt_threadqueue_steal_helper(shared_stack, nostealbuffer,
                                            stealbuffer, amtStolen);

    for(i = 0; (i < local_length) && (amtStolen < maxStolen); i++) {
        amtStolen = qt_threadqueue_steal_helper(&victim_queue->local[i]->stack,
                                                nostealbuffer,
                                                stealbuffer,
                                                amtStolen);
    }

    if (amtStolen < maxStolen) {
        victim_queue->empty = 1;
    }

#ifdef STEAL_PROFILE    // should give mechanism to make steal profiling optional
    qthread_incr(&victim_queue->creator_ptr->steal_amount_stolen, amtStolen);
#endif

    return(amtStolen);
}

/*  Steal work from another shepherd's queue
 *    Returns the amount of work stolen
 */
static QINLINE qthread_t *qthread_steal(qt_threadqueue_t *thiefq)
{   /*{{{*/
    int                  i, j, amtStolen;
    extern pthread_key_t shepherd_structs;
    qthread_shepherd_t  *victim_shepherd;
    qt_threadqueue_t    *victim_queue;
    qthread_worker_t    *worker =
        (qthread_worker_t *)pthread_getspecific(shepherd_structs);
    qthread_shepherd_t *thief_shepherd =
        (qthread_shepherd_t *)worker->shepherd;
    qthread_t **nostealbuffer = worker->nostealbuffer;
    qthread_t **stealbuffer   = worker->stealbuffer;
    int         nshepherds    = qlib->nshepherds;
    int         local_length  = qlib->nworkerspershep + 1;

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

        QTHREAD_TRYLOCK_LOCK(&victim_queue->trylock);
        for (j = 0; j < local_length; j++) {
            QTHREAD_FASTLOCK_LOCK(&victim_queue->local[j]->lock);
        }
        amtStolen = qt_threadqueue_steal(victim_queue, nostealbuffer, stealbuffer);
        for (j = 0; j < local_length; j++) {
            QTHREAD_FASTLOCK_UNLOCK(&victim_queue->local[j]->lock);
        }
        QTHREAD_TRYLOCK_UNLOCK(&victim_queue->trylock);

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
    int i, j;

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

    for (i = 0; i < qlib->nshepherds; i++) {
        for (j = 0; j < qlib->nworkerspershep; j++) {
            fprintf(stdout,
                    "shepherd %d worker %d - enqueue penalty %d bias %d track %d\n", i, j,
                    qlib->shepherds[i].ready->local[j]->penalty,
                    qlib->shepherds[i].ready->local[j]->bias,
                    qlib->shepherds[i].ready->local[j]->track);
        }
    }
}

#endif  /* ifdef STEAL_PROFILE */

/* vim:set expandtab: */
