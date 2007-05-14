#ifndef QTHREAD_INNARDS_H
#define QTHREAD_INNARDS_H

#include <cprops/hashtable.h>

typedef struct qlib_s
{
    unsigned int nshepherds;
    struct qthread_shepherd_s *shepherds;

    unsigned qthread_stack_size;
    unsigned master_stack_size;
    unsigned max_stack_size;

    /* assigns a unique thread_id mostly for debugging! */
    aligned_t max_thread_id;
    pthread_mutex_t max_thread_id_lock;

    /* round robin scheduler - can probably be smarter */
    aligned_t sched_shepherd;
    pthread_mutex_t sched_shepherd_lock;

    /* this is how we manage FEB-type locks
     * NOTE: this can be a major bottleneck and we should probably create
     * multiple hashtables to improve performance. The current hashing is a bit
     * of a hack, but improves the bottleneck a bit
     */
    cp_hashtable *locks[32];
    /* these are separated out for memory reasons: if you can get away with
     * simple locks, then you can use a little less memory. Subject to the same
     * bottleneck concerns as the above hashtable, though these are slightly
     * better at shrinking their critical section. FEBs have more memory
     * overhead, though. */
    cp_hashtable *FEBs[32];
} *qlib_t;

extern qlib_t qlib;

/* These are the internal functions that futurelib should be allowed to get at */
unsigned int qthread_isfuture(const qthread_t * t);
void qthread_assertfuture(qthread_t * t);
void qthread_assertnotfuture(qthread_t * t);
void qthread_fork_future_to(const qthread_f f, const void *arg,
			    aligned_t * ret,
			    const qthread_shepherd_id_t shepherd);

#endif
