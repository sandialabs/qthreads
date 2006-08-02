#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static qthread_lib_t *qlib = NULL;

static void *qthread_shepherd(void *arg)
{
    unsigned *id = (unsigned *)arg;
    qthread_debug("qthread_shepherd(): %d\n", *id);
    qthread_t *t;
    int done=0;

    while(!done) {
        if((t = qthread_dequeue(&qlib->ready, &qlib->ready_lock)) != NULL) {
            if(t->thread_type == QTHREAD_TERM_SHEPHERD) {
                qthread_debug("qthread_shepherd(): returned QTHREAD_TERM_SHEPHERD\n");
                done = 1;
            } else {
                qthread_exec(t);
            }
        }
    }

    qthread_atomic_add(&qlib->nkthreads, &qlib->nkthreads_lock, -1);

    qthread_debug("qthread_shepherd(%p): finished\n", arg);

    pthread_exit(NULL);
}

void qthread_atomic_add(unsigned *x, pthread_mutex_t *lock, int value)
{
    pthread_mutex_lock(lock);
    *x = *x + value;
    pthread_mutex_unlock(lock);
}

unsigned qthread_atomic_check(unsigned *x, pthread_mutex_t *lock)
{
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    pthread_mutex_unlock(lock);

    return(r);
}

static int qthread_lock_cmp(const void *p1, const void *p2, const void *conf)
{
    qthread_lock_t *l1, *l2;

    l1 = (qthread_lock_t *)p1;
    l2 = (qthread_lock_t *)p2;
    if(l1->address < l2->address)
        return(-1);
    if(l1->address > l2->address)
        return(1);
    return(0);
}

qthread_t *qthread_init(int nkthreads)
{
    unsigned i;

    if((qlib = (qthread_lib_t *)malloc(sizeof(qthread_lib_t))) == NULL) {
        perror("qthread_init()");
        abort();
    }

    qlib->ready = NULL;
    qlib->max_thread_id = 0;
    qlib->nthreads = 0;
    qlib->stack_size = QTHREAD_MAX_STACK;

    qlib->main_thread = qthread_new_thread(NULL, NULL);

    if((qlib->locks = rbinit(qthread_lock_cmp, NULL)) == NULL) {
        perror("qthread_init()");
        abort();
    }

    if(pthread_mutex_init(&qlib->nthreads_lock, NULL)) {
        perror("qthread_init()");
        abort();
    }

    if(pthread_mutex_init(&qlib->nkthreads_lock, NULL)) {
        perror("qthread_init()");
        abort();
    }

    if(pthread_mutex_init(&qlib->ready_lock, NULL)) {
        perror("qthread_init()");
        abort();
    }

    if(pthread_mutex_init(&qlib->lock_lock, NULL)) {
        perror("qthread_init()");
        abort();
    }

    qlib->nkthreads = 0;
    if((qlib->kthreads = (pthread_t *)malloc(sizeof(pthread_t) * nkthreads)) 
       == NULL) {
        perror("qthread_init()");
        abort();
    }

    if((qlib->kthread_id = (unsigned *)malloc(sizeof(unsigned) * nkthreads)) 
       == NULL) {
        perror("qthread_init()");
        abort();
    }

    for(i=0; i<nkthreads; i++) {
        qlib->kthread_id[i] = i;
        qthread_atomic_add(&qlib->nkthreads, &qlib->nkthreads_lock, 1);
        if(pthread_create(&qlib->kthreads[i], NULL, qthread_shepherd, (void *)&qlib->kthread_id[i]) != 0) {
            perror("qthread_init(): could not create new thread");
            abort();
        }
    }

    return(qlib->main_thread);
}

void qthread_finalize(void)
{
    int i, r;
    qthread_t *t;
    unsigned nterm;

    assert(qlib != NULL);

    /* the user must ensure that the qthread_fork() function will NOT be called
     * after this point
     */

    nterm = qthread_atomic_check(&qlib->nkthreads, &qlib->nkthreads_lock);

    for(i=0; i<nterm; i++) {
        t = qthread_new_thread(NULL, NULL);
        t->thread_type = QTHREAD_TERM_SHEPHERD;
        qthread_enqueue(&qlib->ready, &qlib->ready_lock, t);
    }

    while(qthread_atomic_check(&qlib->nkthreads, &qlib->nkthreads_lock) != 0)
        ;

    if(pthread_mutex_destroy(&qlib->nthreads_lock)) {
        perror("qthread_finalize()");
        abort();
    }

    if(pthread_mutex_destroy(&qlib->nkthreads_lock)) {
        perror("qthread_finalize()");
        abort();
    }

    if(pthread_mutex_destroy(&qlib->ready_lock)) {
        perror("qthread_finalize()");
        abort();
    }

    if(pthread_mutex_destroy(&qlib->lock_lock)) {
        perror("qthread_finalize()");
        abort();
    }

    rbdestroy(qlib->locks);

    qthread_free_thread(qlib->main_thread);

    free(qlib);
    qlib = NULL;
}

qthread_t *qthread_new_thread(void (*f)(), void *arg)
{
    qthread_t *t;

    if((t = (qthread_t *)malloc(sizeof(qthread_t))) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }

    if(((t->context) = (ucontext_t *)malloc(sizeof(ucontext_t))) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }

    qlib->max_thread_id++;
    t->thread_id = qlib->max_thread_id;
    t->thread_type = QTHREAD_NEW;
    t->f = f;
    t->arg = arg;
    t->stack = NULL;

    return(t);
}

void qthread_new_stack(qthread_t *t)
{
    if((t->stack = (void *)malloc(qlib->stack_size)) == NULL) {
        perror("qthread_new_thread()");
        abort();
    }
}

void qthread_free_thread(qthread_t *t)
{
    free(t->context);
    if(t->stack != NULL)
        free(t->stack);
 
    free(t);
}

void qthread_enqueue(qthread_t **queue, pthread_mutex_t *lock, qthread_t *t)
{
    qthread_t *p;

    if(lock != NULL)
        pthread_mutex_lock(lock);

    qthread_debug("qthread_enqueue(%p, %p, %p)\n", queue, lock, t);

    if(*queue == NULL) {
        *queue = t;
    } else {
        for(p=*queue; p->next != NULL; p = p->next)
            ;
        p->next = t;
        t->next = NULL;
    }

    if(lock != NULL)
        pthread_mutex_unlock(lock);
}

qthread_t *qthread_dequeue(qthread_t **queue, pthread_mutex_t *lock)
{
    qthread_t *t;

    if(lock != NULL) 
        pthread_mutex_lock(lock);

    if(*queue == NULL) {
        if(lock != NULL)
            pthread_mutex_unlock(lock);
        return(NULL);
    }

    t = *queue;
    *queue = (*queue)->next;
    t->next = NULL;

    if(lock != NULL)
        pthread_mutex_unlock(lock);

    qthread_debug("qthread_dequeue(%p, %p): returned %p\n", queue, lock, t);

    return(t);
}

qthread_t *qthread_fork(void (*f)(qthread_t *, void *), void *arg)
{
    qthread_t *t;

    qthread_atomic_add(&qlib->nthreads, &qlib->nthreads_lock, 1);
    t = qthread_new_thread(f, arg);
    qthread_new_stack(t);
    qthread_enqueue(&qlib->ready, &qlib->ready_lock, t);
    return(t);
}

void qthread_exec(qthread_t *t)
{
    ucontext_t c;
    int wasnew;

    qthread_debug("qthread_exec(%p): started\n", t);

    if(t->thread_type == QTHREAD_NEW) {
        t->thread_type = QTHREAD_RUNNING;
        wasnew = 1;

        getcontext(t->context);
        t->context->uc_stack.ss_sp = t->stack;
        t->context->uc_stack.ss_size = qlib->stack_size;
        t->context->uc_link = &c;
        makecontext(t->context, t->f, 2, (void *)t, (void *)t->arg);
    }
     
    /* rcm - maybe this shouldn't return! */

    swapcontext(&c, t->context);

    if(wasnew)
        qthread_free_thread(t);

    qthread_atomic_add(&qlib->nthreads, &qlib->nthreads_lock, -1);
    qthread_debug("qthread_exec(%p): finished\n", t);
}

void qthread_yield()
{
    qthread_t *t;

    if((t = qthread_dequeue(&qlib->ready, &qlib->ready_lock)) == NULL)
        return;

    if(t->thread_type == QTHREAD_TERM_SHEPHERD) {
        qthread_enqueue(&qlib->ready, &qlib->ready_lock, t);
        return;
    }
    qthread_exec(t);
}

void qthread_print_locks()
{
    RBLIST *ptr;
    qthread_lock_t *l;

    assert(qlib != NULL);

    pthread_mutex_lock(&qlib->lock_lock);

    printf("Current Locks:\n");
    if((ptr = rbopenlist(qlib->locks)) != NULL) {
        while((l = (qthread_lock_t *)rbreadlist(ptr)) != NULL)
            printf("\t%p\n", l->address);
        rbcloselist(ptr);
    }
    pthread_mutex_unlock(&qlib->lock_lock);
}

void qthread_print_queue()
{
    qthread_t *t;

    pthread_mutex_lock(&qlib->ready_lock);

    printf("Queue:\n");
    for(t=qlib->ready; t!=NULL; t=t->next) {
        printf("\t%d: type=%d %p\n", t->thread_id, t->thread_type, t->f);
    }

    pthread_mutex_unlock(&qlib->ready_lock);
}

unsigned qthread_get_id(qthread_t *t)
{
    return(t->thread_id);
}

void qthread_lock(qthread_t *t, void *a)
{
    static qthread_lock_t *l=NULL;
    qthread_lock_t *m;

    pthread_mutex_lock(&qlib->lock_lock);

    m = NULL;

    if(l == NULL) {
        if((l = (qthread_lock_t *)malloc(sizeof(qthread_lock_t))) == NULL) {
            perror("qthread_lock()");
            abort();
        }
        l->locked = 0;
    }

    l->address = a;

    m = (qthread_lock_t *)rbsearch((void *)l, qlib->locks);

    assert(m != NULL);
    if(l == m)
        l = NULL;

    /* now that we know which lock to test, try it */
    if(!m->locked) {
        m->owner = t->thread_id;
        qthread_debug("qthread_lock(%p, %p): returned (not locked)\n", t, a);

        pthread_mutex_unlock(&qlib->lock_lock);
        return;
    }

    /* failure, dequeue this thread and yield */
    qthread_enqueue(&m->waiting, NULL, t);

    pthread_mutex_unlock(&qlib->lock_lock);
    qthread_debug("qthread_lock(%p, %p): executing qthread_yield()\n", t, a);
    qthread_yield();
    /* note: when the thread runs again, it has the lock! */
    m->owner = t->thread_id;

    qthread_debug("qthread_lock(%p, %p): returned (was locked)\n", t, a);

}

void qthread_unlock(qthread_t *t, void *a)
{
    qthread_lock_t l, *m;
    qthread_t *u;

    pthread_mutex_lock(&qlib->lock_lock);

    l.address = a;
    qthread_debug("qthread_unlock(%p, %p): started\n", t, a);

    m = (qthread_lock_t *)rbfind((void *)&l, qlib->locks);
    if(m == NULL) {
        return;
        /* rcm below might be the right behavior! */
        fqthread_debug(stderr, "qthread_lock(): attempt by thread %d to unlock a "
                "non-existent lock (%p)\n", t->thread_id, a);
        abort();
    }

    /* rcm - the mutex is already locked and doesn't care who unlocks it, so... */

    u = qthread_dequeue(&m->waiting, NULL);
    if(u != NULL) {
        m->owner = t->thread_id;
        qthread_enqueue(&qlib->ready, &qlib->ready_lock, u);
    } else {
        rbdelete(m, qlib->locks);
    }
    pthread_mutex_unlock(&qlib->lock_lock);

    qthread_debug("qthread_unlock(%p, %p): returned\n", t, a);
}



