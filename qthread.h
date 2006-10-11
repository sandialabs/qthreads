#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#ifdef MEMWATCH
#include "memwatch.h"
#endif

#include <ucontext.h>
#include <pthread.h>
#include <stdarg.h>
#include <cprops/hashtable.h>

#define QTHREAD_DEFAULT_STACK_SIZE 2048

#define QTHREAD_STATE_NEW               0
#define QTHREAD_STATE_RUNNING           1
#define QTHREAD_STATE_YIELDED           2
#define QTHREAD_STATE_BLOCKED           3
#define QTHREAD_STATE_TERMINATED        4
#define QTHREAD_STATE_DONE              5
#define QTHREAD_STATE_TERM_SHEP         0xFFFFFFFF

struct qthread_lock_s;

typedef struct qthread_s {
    unsigned thread_id;
    unsigned thread_state;

    struct qthread_lock_s *blockedon;                  /* when yielding because blocked
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
    pthread_mutex_t lock;
    qthread_queue_t *waiting;
} qthread_lock_t;

typedef struct {
    int nkthreads;
    qthread_shepherd_t *kthreads;

    unsigned qthread_stack_size;
    unsigned master_stack_size;
    unsigned max_stack_size;

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
#ifdef RBTREE
    struct rbtree *locks;
#else
    cp_hashtable *locks;
#endif
} qlib_t;

/* for convenient arguments to qthread_fork */
typedef void (*qthread_f)(qthread_t * t);

int qthread_init(int nkthreads);
void qthread_finalize(void);

void qthread_yield(qthread_t *t);

qthread_t *qthread_fork(qthread_f f, void *arg);
void qthread_join(qthread_t *me, qthread_t *waitfor);
void qthread_busy_join(volatile qthread_t *waitfor);

int qthread_lock(qthread_t *t, void *a);
int qthread_unlock(qthread_t *t, void *a);

#endif /* _QTHREAD_H_ */

