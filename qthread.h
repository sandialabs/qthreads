#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <pthread.h>		       /* included here only as a convenience */

typedef struct qthread_s qthread_t;

/* for convenient arguments to qthread_fork */
typedef void (*qthread_f) (qthread_t * t);

/* use this function to initialize the qthreads environment before spawning any
 * qthreads. The argument to this function specifies the number of pthreads
 * that will be spawned to shepherd the qthreads. */
int qthread_init(int nkthreads);

/* use this function to clean up the qthreads environment after execution of
 * the program is finished. This function will terminate any currently running
 * qthreads, so only use it when you are certain that execution has completed.
 * For examples of how to do this, look at the included test programs. */
void qthread_finalize(void);

/* this function allows a qthread to specifically give up control of the
 * processor even though it has not blocked. This is useful for things like
 * busy-waits or cooperative multitasking. Without this function, threads will
 * only ever allow other threads assigned to the same pthread to execute when
 * they block. */
void qthread_yield(qthread_t * t);

/* these are the functions for generating a new qthread. The specified function
 * will be run to completion. The difference between them is that a detached
 * qthread cannot be joined, but an un-detached qthread MUST be joined
 * (otherwise its memory will not be free'd). The qthread_fork_to* functions
 * spawn the thread to a specific shepherd */
qthread_t *qthread_fork(qthread_f f, void *arg);
qthread_t *qthread_fork_to(qthread_f f, void *arg, const unsigned int shepherd);
void qthread_fork_detach(qthread_f f, void *arg);
void qthread_fork_to_detach(qthread_f f, void *arg, const unsigned int shepherd);

/* these are accessor functions for use by the qthreads to retrieve information
 * about themselves */
unsigned qthread_id(qthread_t * t);
void *qthread_arg(qthread_t * t);

/* These are the join functions, which will only return once the specified
 * thread has finished executing.
 *
 * The standard qthread_join() only works from within a qthread; it uses
 * qthread_lock/unlock, so it's a blocking join that will not take processing
 * time. The thread will be queued and will allow other threads to execute.
 *
 * The qthread_busy_join() function will work from a non-qthread thread
 * (including the main thread), however since it cannot queue itself (there is
 * no qthread context), it must keep checking for the return value in a tight
 * loop. (Note: this could be accomplished using a pthread_mutex_t for every thread,
 * but that would inflate the qthread_t memory requirements unnecessarily.)
 */
void qthread_join(qthread_t * me, qthread_t * waitfor);
void qthread_busy_join(volatile qthread_t * waitfor);

/* functions to implement FEB locking/unlocking
 *
 * These are the FEB functions. All but empty/fill have the potential of
 * blocking until the corresponding precondition is met. All FEB blocking is
 * done on a byte-by-byte basis, which means that every single address *could*
 * have a lock associated with it. Memory is assumed to be full unless
 * otherwise asserted, and as such memory that is full and does not have
 * dependencies (i.e. no threads are waiting for it to become empty) does not
 * require state data to be stored. It is expected that while there may be
 * locks instantiated at one time or another for a very large number of
 * addresses in the system, relatively few will be in a non-default (full, no
 * waiters) state at any one time.
 */

/* The empty/fill functions merely assert the empty or full state of the given
 * range of addresses */
void qthread_empty(qthread_t * t, char *dest, const size_t bytes);
void qthread_fill(qthread_t * t, char *dest, const size_t bytes);

/* These functions wait for memory to become empty, and then fill it. When
 * memory becomes empty, only one thread blocked like this will be awoken. Data
 * is read from src and written to dest.
 *	writeEF(t, &dest, src)
 * is roughly equivalent to
 *	writeEF_size(t, &dest, &src, sizeof(int))
 */
void qthread_writeEF(qthread_t * t, int *dest, int src);
void qthread_writeEF_size(qthread_t * t, char *dest, char *src,
			  const size_t bytes);

/* These functions wait for memory to become full, and then read it and leave
 * the memory as full. When memory becomes full, all threads waiting for it to
 * become full with a readFF will receive the value at once and will be queued
 * to run. Data is read from src and stored in dest.
 *	readFF(t, &dest, src)
 * is roughly equivalent to
 *	readFF_size(t, &dest, &src, sizeof(int))
 */
void qthread_readFF(qthread_t * t, int *dest, int src);
void qthread_readFF_size(qthread_t * t, char *dest, char *src,
			 const size_t bytes);

/* These functions wait for memory to become full, and then empty it. When
 * memory becomes full, only one thread blocked like this will be awoken. Data
 * is read from src and written to dest.
 *	readFE(t, &dest, src)
 * is roughly equivalent to
 *	readFE_size(t, &dest, &src, sizeof(int))
 */
void qthread_readFE(qthread_t * t, int *dest, int src);
void qthread_readFE_size(qthread_t * t, char *dest, char *src,
			 const size_t bytes);

/* functions to implement FEB-ish locking/unlocking
 *
 * These are atomic and functional, but do not have the same semantics as full
 * FEB locking/unlocking (for example, unlocking cannot block, and lock ranges
 * are not supported), however because of this, they have lower overhead.
 */
int qthread_lock(qthread_t * t, void *a);
int qthread_unlock(qthread_t * t, void *a);

#endif /* _QTHREAD_H_ */
