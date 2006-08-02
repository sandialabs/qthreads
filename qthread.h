#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <ucontext.h>
#include <pthread.h>
#include <stdarg.h>
#include "redblack.h"

//#define QTHREAD_DEBUG 1

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

#define QTHREAD_DEFAULT_STACK_SIZE      8192

#define QTHREAD_THREAD_NEW              0
#define QTHREAD_THREAD_RUNNING          1
#define QTHREAD_THREAD_TERM_SHEP        2

typedef struct qthread_s {
    unsigned thread_id;
    unsigned thread_type;

    void (*f)(struct qthread_s *);              /* the function to call */
    void *arg;                                  /* user defined data */

    ucontext_t *context;                        /* the context switch info */
    void *stack;                                /* the thread's stack */

    struct qthread_s *next;
} qthread_t;

typedef struct {
    pthread_t kthread;
    qthread_t *ready_head;
    qthread_t *ready_tail;
    pthread_mutex_t ready_lock;
    pthread_cond_t ready_notempty;
} qthread_shepherd_t;

typedef struct {
    int nkthreads;
    qthread_shepherd_t *kthreads;

    unsigned stack_size;
    
    /* assigns a unique thread_id mostly for debugging! */
    unsigned max_thread_id;
    pthread_mutex_t max_thread_id_lock;
    
    /* round robin scheduler - can probably be smarter */
    unsigned sched_kthread;
    pthread_mutex_t sched_kthread_lock;
} qlib_t;

/* public functions */

int qthread_init(int nkthreads);
void qthread_finalize(void);
qthread_t *qthread_fork(void (*f)(qthread_t *), void *arg);

static inline void *qthread_get_arg(qthread_t *t) { return (t->arg); }

/* private functions */

void qthread_enqueue(qthread_t **head, qthread_t **tail, pthread_mutex_t *lock, 
                     pthread_cond_t *notempty, qthread_t *t);
qthread_t *qthread_dequeue(qthread_t **head, qthread_t **tail, 
                           pthread_mutex_t *lock, pthread_cond_t *notempty);

qthread_t *qthread_new_thread(void (*f)(qthread_t *), void *arg);
void qthread_free_thread(qthread_t *t);
void qthread_new_stack(qthread_t *t);
void qthread_free_stack(qthread_t *t);

/* private functions that need to be inlined! */
static inline unsigned qthread_atomic_inc(unsigned *x, pthread_mutex_t *lock, 
                                         int inc)
{
    unsigned r;
    pthread_mutex_lock(lock);
    r = *x;
    *x = *x + inc;
    pthread_mutex_unlock(lock);
    return(r);
}

static inline unsigned qthread_atomic_inc_mod(unsigned *x, pthread_mutex_t *lock,
                                             int inc, int mod)
{
    unsigned r;
    pthread_mutex_lock(lock);
    r = *x;
    *x = (*x + inc) % mod;
    pthread_mutex_unlock(lock);
    return(r);
}

static inline unsigned qthread_atomic_check(unsigned *x, pthread_mutex_t *lock)
{
    unsigned r;

    pthread_mutex_lock(lock);
    r = *x;
    pthread_mutex_unlock(lock);

    return(r);
}


#endif /* _QTHREAD_H_ */
