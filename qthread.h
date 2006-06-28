#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <ucontext.h>
#include <pthread.h>
#include "redblack.h"

#define QTHREAD_MAX_STACK       8192

typedef struct qthread_s {
    unsigned thread_id;
    unsigned isnew;

    void (*f)(struct qthread_s *, void *);     /* the function to call */
    void *arg;                          /* its argument */

    ucontext_t *context;
    void *stack;

    struct qthread_s *next;
} qthread_t;

typedef struct {
    void *address;
    unsigned owner;
    pthread_mutex_t lock;
    pthread_mutex_t waiting_lock;
    qthread_t *waiting;
} qthread_lock_t;

typedef struct {
    unsigned max_thread_id;
    unsigned nkthreads;
    size_t stack_size;

    pthread_t *kthreads;
    unsigned *kthread_id;

    pthread_mutex_t ready_lock;
    qthread_t *ready;

    pthread_mutex_t lock_lock;          /* rcm - get rid of me! */
    struct rbtree *locks;
} qthread_lib_t;

int qthread_init(int nkthreads);
void qthread_finalize(void);

void qthread_fork(void (*f)(qthread_t *, void *), void *arg);
void qthread_yield();

void qthread_lock(qthread_t *t, void *a);
void qthread_unlock(qthread_t *t, void *a);

unsigned qthread_get_id(qthread_t *t);

/* internals and candidates for inlining */
qthread_t *qthread_new_thread(void (*f)(), void *arg);
void qthread_new_stack(qthread_t *t);
void qthread_free_thread(qthread_t *t);

void qthread_enqueue(qthread_t **queue, pthread_mutex_t *lock, qthread_t *t);
qthread_t *qthread_dequeue(qthread_t **queue, pthread_mutex_t *lock);

void qthread_exec(qthread_t *t);

void qthread_print_locks();
void qthread_print_queue();

#endif /* _QTHREAD_H_ */
