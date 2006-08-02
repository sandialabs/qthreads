#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static qlib_t *qlib=NULL;

static void *qthread_shepherd(void *arg)
{
    qthread_shepherd_t *me = (qthread_shepherd_t *)arg;
    qthread_t *t;
    int done=0;
    
    qthread_debug("qthread_shepherd(%p): forked\n", me);

    while(!done) {
        t = qthread_dequeue(&me->ready_head, &me->ready_tail, &me->ready_lock, 
                            &me->ready_notempty);

        qthread_debug("qthread_shepherd(%p): dequeued thread id %d/type %d\n", me, t->thread_id, t->thread_type);

        switch(t->thread_type) {
            case QTHREAD_THREAD_NEW:
            case QTHREAD_THREAD_RUNNING:
                assert(t->f != NULL);
                (*t->f)(t);
                break;
            case QTHREAD_THREAD_TERM_SHEP:
                done = 1;
                break;
            default:
                fprintf(stderr, "qthread_shepherd(): unknown type %d/%d\n",
                        t->thread_id, t->thread_type);
                abort();
        }

        /* thread is done */
        qthread_free_thread(t);
    }

    qthread_debug("qthread_shepherd(%p): finished\n", me);
}

/* PUBLIC FUNCTIONS */

int qthread_init(int nkthreads)
{
    int i, r;

    qthread_debug("qthread_init(): began.\n");

    if((qlib = (qlib_t *)malloc(sizeof(qlib_t))) == NULL) {
        perror("qthread_init()");
        abort();
    }

    qlib->nkthreads = nkthreads;
    if((qlib->kthreads = (qthread_shepherd_t *)
        malloc(sizeof(qthread_shepherd_t)*nkthreads)) == NULL) {
        perror("qthread_init()");
        abort();
    }

    qlib->stack_size = QTHREAD_DEFAULT_STACK_SIZE;
    qlib->sched_kthread = 0;
    assert(pthread_mutex_init(&qlib->sched_kthread_lock, NULL) == 0);

    for(i=0; i<nkthreads; i++) {
        qlib->kthreads[i].ready_head = NULL;
        qlib->kthreads[i].ready_tail = NULL;
        assert(pthread_mutex_init(&qlib->kthreads[i].ready_lock, NULL) == 0);
        assert(pthread_cond_init(&qlib->kthreads[i].ready_notempty, NULL) == 0);

        qthread_debug("qthread_init(): forking thread %p\n", &qlib->kthreads[i]);

        if((r = pthread_create(&qlib->kthreads[i].kthread, NULL, 
                               qthread_shepherd, &qlib->kthreads[i])) != 0) {
            fprintf(stderr, "qthread_init: pthread_create() failed (%d)\n", r);
            abort();
        }
    }
    
    qthread_debug("qthread_init(): finished.\n");
}

void qthread_finalize(void)
{
    int i, r;
    qthread_t *t;

    assert(qlib != NULL);

    qthread_debug("qthread_finalize(): began.\n");

    /* rcm - probably need to put a "turn off the library flag" here, but,
     * the programmer can ensure that no further threads are forked for now
     */

    /* enqueue the termination thread sentinal */
    for(i=0; i<qlib->nkthreads; i++) {
        t = qthread_new_thread(NULL, NULL);
        t->thread_type = QTHREAD_THREAD_TERM_SHEP;
        qthread_enqueue(&qlib->kthreads[i].ready_head, 
                        &qlib->kthreads[i].ready_tail, 
                        &qlib->kthreads[i].ready_lock, 
                        &qlib->kthreads[i].ready_notempty, t);
    }

    /* wait for each thread to drain it's queue! */
    for(i=0; i<qlib->nkthreads; i++) {
        if((r = pthread_join(qlib->kthreads[i].kthread, NULL)) != 0) {
            fprintf(stderr, "qthread_finalize: pthread_join() failed (%d)\n",r);
            abort();
        }
    }

    free(qlib->kthreads);
    free(qlib);
    qlib = NULL;

    qthread_debug("qthread_finalize(): finished.\n");
}

qthread_t *qthread_fork(void (*f)(qthread_t *), void *arg)
{
    qthread_t *t;
    unsigned shep;

    t = qthread_new_thread(f, arg);
    qthread_new_stack(t);
    
    /* figure out which queue to put the thread into */
    shep = qthread_atomic_inc_mod(&qlib->sched_kthread, 
                                  &qlib->sched_kthread_lock, 1, 
                                  qlib->nkthreads);

    qthread_enqueue(&qlib->kthreads[shep].ready_head, 
                    &qlib->kthreads[shep].ready_tail, 
                    &qlib->kthreads[shep].ready_lock, 
                    &qlib->kthreads[shep].ready_notempty, t);

    return(t);
}


/* PRIVATE FUNCTIONS  */

/* functions to manage the thread queues */

void qthread_enqueue(qthread_t **head, qthread_t **tail, pthread_mutex_t *lock, 
                     pthread_cond_t *notempty, qthread_t *t)
{
    assert(t != NULL);
    assert(pthread_mutex_lock(lock) == 0);

    t->next = NULL;

    if((*head == NULL) && (*tail == NULL)) {
        *head = t;
        *tail = t;
        assert(pthread_cond_signal(notempty) == 0);
    } else {
        (*tail)->next = t;
        *tail = t;
    }

    assert(pthread_mutex_unlock(lock) == 0);
}

qthread_t *qthread_dequeue(qthread_t **head, qthread_t **tail, 
                           pthread_mutex_t *lock, pthread_cond_t *notempty)
{
    qthread_t *t;

    assert(pthread_mutex_lock(lock) == 0);

    while((*head == NULL) && (*tail == NULL)) {
        assert(pthread_cond_wait(notempty, lock) == 0);
    }

    assert(*head != NULL);

    if(*head != *tail) {
        t = *head;
        *head = (*head)->next;
        t->next = NULL;
    } else {
        t = *head;
        *head = (*head)->next;
        t->next = NULL;
        *tail = NULL;
    }

    assert(pthread_mutex_unlock(lock) == 0);

    return(t);
}

/* functions to manage thread state allocation/deallocation */

qthread_t *qthread_new_thread(void (*f)(), void *arg)
{
    qthread_t *t;

    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    if((t = (qthread_t *)malloc(sizeof(qthread_t))) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }

    if(((t->context) = (ucontext_t *)malloc(sizeof(ucontext_t))) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }

    t->thread_id = qthread_atomic_inc(&qlib->max_thread_id, 
                                      &qlib->max_thread_id_lock, 1);
    t->thread_type = QTHREAD_THREAD_NEW;
    t->f = f;
    t->arg = arg;
    t->stack = NULL;

    return(t);
}

void qthread_free_thread(qthread_t *t)
{
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    free(t->context);
    qthread_free_stack(t);
 
    free(t);
}

void qthread_new_stack(qthread_t *t)
{
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */

    if((t->stack = (void *)malloc(qlib->stack_size)) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }
}

void qthread_free_stack(qthread_t *t)
{
    /* rcm - note: in the future we REALLY want to reuse thread and 
     * stack allocations! 
     */
    if(t->stack != NULL)
        free(t->stack);
}

