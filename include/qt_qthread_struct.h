#ifndef QT_QTHREAD_STRUCT_H
#define QT_QTHREAD_STRUCT_H

enum threadstate {
    QTHREAD_STATE_NEW,
    QTHREAD_STATE_RUNNING,
    QTHREAD_STATE_YIELDED,
    QTHREAD_STATE_BLOCKED,
    QTHREAD_STATE_FEB_BLOCKED,
    QTHREAD_STATE_TERMINATED,
    QTHREAD_STATE_DONE,
    QTHREAD_STATE_MIGRATING,
    QTHREAD_STATE_TERM_SHEP = UINT8_MAX };

struct qthread_runtime_data_s
{
    void *stack;		/* the thread's stack */
    ucontext_t context;	        /* the context switch info */

    ucontext_t *return_context;	/* context of parent shepherd */

    /* a pointer used for passing information back to the shepherd when
     * becoming blocked */
    struct qthread_lock_s *blockedon;
    qthread_shepherd_t *shepherd_ptr; /* the shepherd we run on */

#ifdef QTHREAD_USE_VALGRIND
    unsigned int valgrind_stack_id;
#endif
#ifdef QTHREAD_USE_ROSE_EXTENSIONS
    int forCount; /* added akp */
    taskSyncvar_t *openmpTaskRetVar; /* ptr to linked list if task's I started -- used in openMP taskwait */
    syncvar_t taskWaitLock;
#endif
};

struct qthread_s
{
    struct qthread_s *next;
    /* the shepherd our memory comes from */
    qthread_shepherd_t *creator_ptr;

    unsigned int thread_id;
    enum threadstate thread_state;
    unsigned char flags;

    /* the shepherd we'd rather run on */
    qthread_shepherd_t *target_shepherd;

    /* the function to call (that defines this thread) */
    qthread_f f;
    aligned_t id;               /* id used in barrier and arrive_first */
    void *arg;			/* user defined data */
    void *ret;			/* user defined retval location */
    aligned_t   free_arg;       /* user defined data malloced and to be freed */
    aligned_t   test[128];      /* space so that I can avoid malloc in most small cases */
    struct qthread_runtime_data_s *rdata;
};

#endif
