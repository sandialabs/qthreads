#include <stdio.h>
#include <assert.h>
#include "qthread.h"

static qthread_lib_t *qlib = NULL;

static void *qthread_shepherd(void *arg)
{
    unsigned *id = (unsigned *)arg;
    printf("qthread_shepherd(): %d\n", *id);

    while(1) {
        qthread_yield();
    }

    return(NULL);
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

int qthread_init(int nkthreads)
{
    unsigned i;

    if((qlib = (qthread_lib_t *)malloc(sizeof(qthread_lib_t))) == NULL) {
        perror("qthread_init()");
        abort();
    }

    qlib->ready = NULL;
    qlib->max_thread_id = 0;
    qlib->stack_size = QTHREAD_MAX_STACK;
    if((qlib->locks = rbinit(qthread_lock_cmp, NULL)) == NULL) {
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

    qlib->nkthreads = nkthreads;
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

    for(i=0; i<qlib->nkthreads; i++) {
        qlib->kthread_id[i] = i;
        if(pthread_create(&qlib->kthreads[i], NULL, qthread_shepherd, (void *)&qlib->kthread_id[i]) != 0) {
            perror("qthread_init(): could not create new thread");
            abort();
        }
    }

    return(0);
}

void qthread_finalize(void)
{
    int i;

    assert(qlib != NULL);

    /* keep running threads until we're done */

    for(i=0; i<qlib->nkthreads; i++)
        pthread_join(qlib->kthreads[i], NULL);

    if(pthread_mutex_destroy(&qlib->ready_lock)) {
        perror("qthread_finalize()");
        abort();
    }
    if(pthread_mutex_destroy(&qlib->lock_lock)) {
        perror("qthread_finalize()");
        abort();
    }

    rbdestroy(qlib->locks);

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
    t->isnew = 1;
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
    pthread_mutex_lock(lock);

    if(*queue == NULL) {
        *queue = t;
    } else {
        t->next = *queue;
        *queue = t;
    }

    pthread_mutex_unlock(lock);
}

qthread_t *qthread_dequeue(qthread_t **queue, pthread_mutex_t *lock)
{
    qthread_t *t;

    pthread_mutex_lock(lock);

    if(*queue == NULL)
        return(NULL);

    t = *queue;
    *queue = (*queue)->next;

    pthread_mutex_unlock(lock);

    return(t);
}

void qthread_fork(void (*f)(qthread_t *, void *), void *arg)
{
    qthread_t *t;

    t = qthread_new_thread(f, arg);
    qthread_new_stack(t);
    qthread_enqueue(&qlib->ready, &qlib->ready_lock, t);
}

void qthread_exec(qthread_t *t)
{
    ucontext_t c;
    int wasnew;

    if(t->isnew) {
        t->isnew = 0;
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
}

void qthread_yield()
{
    qthread_t *t;

    if((t = qthread_dequeue(&qlib->ready, &qlib->ready_lock)) == NULL)
        return;

    qthread_exec(t);
}

void qthread_print_locks()
{
    RBLIST *ptr;
    qthread_lock_t *l;

    assert(qlib != NULL);

    printf("Current Locks:\n");
    if((ptr = rbopenlist(qlib->locks)) != NULL) {
        while((l = (qthread_lock_t *)rbreadlist(ptr)) != NULL)
            printf("\t%p\n", l->address);
        rbcloselist(ptr);
    }
}

void qthread_print_queue()
{
    qthread_t *t;

    pthread_mutex_lock(&qlib->ready_lock);

    printf("Queue:\n");
    for(t=qlib->ready; t!=NULL; t=t->next) {
        printf("\t%d: %c %p\n", t->thread_id, t->isnew? 'N':'O', t->f);
    }

    pthread_mutex_unlock(&qlib->ready_lock);
}

unsigned qthread_get_id(qthread_t *t)
{
    return(t->thread_id);
}

/* rcm - fix this! */

void qthread_lock(qthread_t *t, void *a)
{
    static qthread_lock_t *l=NULL;
    qthread_lock_t *m=NULL;

    if(l == NULL) {
        if((l = (qthread_lock_t *)malloc(sizeof(qthread_lock_t))) == NULL) {
            perror("qthread_lock()");
            abort();
        }
        pthread_mutex_init(&l->lock, NULL);
        pthread_mutex_init(&l->waiting_lock, NULL);
    }

    l->address = a;

    pthread_mutex_lock(&qlib->lock_lock);
    m = (qthread_lock_t *)rbsearch((void *)l, qlib->locks);

    assert(m != NULL);
    if(l == m) {
        l = NULL;
    }

    /* now that we know which lock to test, try it */
    if(pthread_mutex_trylock(&m->lock) == 0) {
        /* the lock has been acquired */
        m->owner = t->thread_id;
        pthread_mutex_unlock(&qlib->lock_lock);
    } else {
        /* failure, dequeue this thread */
        qthread_enqueue(&qlib->ready, &qlib->ready_lock, t);
        pthread_mutex_unlock(&qlib->lock_lock);
        qthread_yield();
        /* note: when the thread runs again, it has the lock! */
        m->owner = t->thread_id;
    }

}

void qthread_unlock(qthread_t *t, void *a)
{
    qthread_lock_t l, *m;
    qthread_t *u;

    l.address = a;

    pthread_mutex_lock(&qlib->lock_lock);
    m = (qthread_lock_t *)rbfind((void *)&l, qlib->locks);
    if(m == NULL) {
        fprintf(stderr, "qthread_lock(): attempt by thread %d to unlock a "
                "non-existent lock (%p)\n", t->thread_id, a);
        abort();
    }

    pthread_mutex_unlock(&m->lock);      /* rcm - this could be bad if another thread picked up the work! */
    pthread_mutex_lock(&m->lock);

    u = qthread_dequeue(&m->waiting, &m->waiting_lock);
    if(u != NULL) {
        m->owner = t->thread_id;
        qthread_enqueue(&qlib->ready, &qlib->ready_lock, u);
    } else {
        rbdelete(m, qlib->locks);
    }

    pthread_mutex_unlock(&qlib->lock_lock);
}



