#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <ucontext.h>
#include <pthread.h>
#include <stdarg.h>
#include "redblack.h"

#define QTHREAD_NEW             0
#define QTHREAD_RUNNING         1
#define QTHREAD_TERM_SHEPHERD   2

#define QTHREAD_MAX_STACK       8192

#define QTHREAD_DEBUG 1

/* for debugging */
#ifdef QTHREAD_DEBUG
        static inline void qthread_debug(char *format, ...)
        {
            static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;
            va_list args;

            pthread_mutex_lock(&output_lock);
            
            fprintf(stderr, "qthread_debug(): ");

            va_start(args, format);
            vfprintf(stderr, format, args);
            va_end(args);

            pthread_mutex_unlock(&output_lock);
        }
#else
        #define qthread_debug(format, ...) ((void)0)
#endif 

typedef struct qthread_s {
    unsigned thread_id;
    unsigned thread_type;

    void (*f)(struct qthread_s *, void *);     /* the function to call */
    void *arg;                          /* its argument */

    ucontext_t *context;
    void *stack;

    struct qthread_s *next;
} qthread_t;

typedef struct {
    void *address;
    unsigned owner;
    unsigned locked;
    qthread_t *waiting;
} qthread_lock_t;

typedef struct {
    unsigned max_thread_id;
    unsigned nthreads;
    pthread_mutex_t nthreads_lock;
    unsigned nkthreads;
    pthread_mutex_t nkthreads_lock;
    size_t stack_size;
    unsigned done;

    qthread_t *main_thread;

    pthread_t *kthreads;
    unsigned *kthread_id;

    pthread_mutex_t ready_lock;
    qthread_t *ready;

    pthread_mutex_t lock_lock;          /* rcm - get rid of me! */
    struct rbtree *locks;
} qthread_lib_t;

/* public functions */
qthread_t *qthread_init(int nkthreads);
void qthread_finalize(void);

qthread_t *qthread_fork(void (*f)(qthread_t *, void *), void *arg);
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

void qthread_atomic_add(unsigned *x, pthread_mutex_t *lock, int value);
unsigned qthread_atomic_check(unsigned *x, pthread_mutex_t *lock);


#endif /* _QTHREAD_H_ */
