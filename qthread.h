#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <ucontext.h>
#include <pthread.h>
#include <stdarg.h>
#include "redblack.h"

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

#define QTHREAD_DEFAULT_STACK_SIZE      8192

#define QTHREAD_STATE_NEW               0
#define QTHREAD_STATE_RUNNING           1
#define QTHREAD_STATE_YIELDED           2
#define QTHREAD_STATE_BLOCKED           3
#define QTHREAD_STATE_TERMINATED        4
#define QTHREAD_STATE_DONE              5
#define QTHREAD_STATE_TERM_SHEP         0xFFFFFFFF

typedef struct qthread_s {
    unsigned thread_id;
    unsigned thread_state;

    void *queue;                                /* when yielding because blocked
                                                 * this is the waiting queue
                                                 */

    unsigned shepherd;                          /* the pthread we run on */

    void (*f)(struct qthread_s *);              /* the function to call */
    void *arg;                                  /* user defined data */

    ucontext_t *context;                        /* the context switch info */
    void *stack;                                /* the thread's stack */
    ucontext_t *return_context;                 /* context of parent kthread */

    struct qthread_s *next;
} qthread_t;

typedef struct {
    qthread_t *head;
    qthread_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t notempty;
} qthread_queue_t;

typedef struct {
    pthread_t kthread;
    unsigned kthread_index;
    qthread_queue_t *ready;
} qthread_shepherd_t;

typedef struct qthread_lock_s {
    void *address;
    unsigned owner;
    unsigned locked;
    qthread_queue_t *waiting;
} qthread_lock_t;

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

    /* this is how we manage FEB-type locks
     * NOTE: this is a major bottleneck and we should probably hash on the
     * lower-order bits of the lock address to improve performance
     */
    pthread_mutex_t lock_lock;
    struct rbtree *locks;
} qlib_t;

int qthread_init(int nkthreads);
void qthread_finalize(void);

qthread_t *qthread_thread_new(void (*f)(), void *arg);
void qthread_thread_free(qthread_t *t);
void qthread_stack_new(qthread_t *t, unsigned stack_size);
void qthread_stack_free(qthread_t *t);

qthread_queue_t *qthread_queue_new();
void qthread_queue_free(qthread_queue_t *q);

void qthread_enqueue(qthread_queue_t *q, qthread_t *t);
qthread_t *qthread_dequeue(qthread_queue_t *q);
qthread_t *qthread_dequeue_nonblocking(qthread_queue_t *q);

void qthread_exec(qthread_t *t, ucontext_t *c);
void qthread_yield(qthread_t *t);

qthread_t *qthread_fork(void (*f)(qthread_t *), void *arg);
void qthread_join(qthread_t *t);

void qthread_lock(qthread_t *t, void *a);
void qthread_unlock(qthread_t *t, void *a);

/* functions that need to be inlined! */
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

