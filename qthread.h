#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <pthread.h>		       /* included here only as a convenience */

#include <qthread-int.h>	       /* for uint32_t and uint64_t */

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct qthread_s qthread_t;

/* for convenient arguments to qthread_fork */
typedef void (*qthread_f) (qthread_t * me);

/* FEB locking only works on aligned addresses. On 32-bit architectures, this
 * isn't too much of an inconvenience. On 64-bit architectures, it's a pain in
 * the BUT! This is here to try and help a little bit. */
#ifdef __LP64__
typedef uint64_t aligned_t;
#else
typedef uint32_t aligned_t;
#endif

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
void qthread_yield(qthread_t * me);

/* this function allows a qthread to retrive it's qthread_t pointer if it has
 * been lost for some reason */
qthread_t *qthread_self(void);

/* these are the functions for generating a new qthread. The specified function
 * (the first argument; note that it is a qthread_f and not a qthread_t)
 * will be run to completion. The difference between them is that a detached
 * qthread cannot be joined, but an un-detached qthread MUST be joined
 * (otherwise not all of its memory will be free'd). The qthread_fork_to*
 * functions spawn the thread to a specific shepherd */
qthread_t *qthread_fork(qthread_f f, void *arg);
qthread_t *qthread_fork_to(qthread_f f, void *arg,
			   const unsigned int shepherd);
void qthread_fork_detach(qthread_f f, void *arg);
void qthread_fork_to_detach(qthread_f f, void *arg,
			    const unsigned int shepherd);

/* these are accessor functions for use by the qthreads to retrieve information
 * about themselves */
unsigned qthread_id(qthread_t * t);
void *qthread_arg(qthread_t * t);

/* This is the join function, which will only return once the specified thread
 * has finished executing.
 *
 * qthread_join() only works whether within a qthread or not (if not, pass it
 * NULL for the "me" argument). It relies on qthread_lock/unlock of the
 * qthread_t data-structure address (the user CAN muck with that, but it's not
 * recommended), so it's a blocking join that will not take processing
 * time.
 */
void qthread_join(qthread_t * me, qthread_t * waitfor);

/****************************************************************************
 * functions to implement FEB locking/unlocking
 ****************************************************************************
 *
 * These are the FEB functions. All but empty/fill have the potential of
 * blocking until the corresponding precondition is met. All FEB
 * blocking/reading/writing is done on a machine-word basis. Memory is assumed
 * to be full unless otherwise asserted, and as such memory that is full and
 * does not have dependencies (i.e. no threads are waiting for it to become
 * empty) does not require state data to be stored. It is expected that while
 * there may be locks instantiated at one time or another for a very large
 * number of addresses in the system, relatively few will be in a non-default
 * (full, no waiters) state at any one time.
 */

/* The empty/fill functions merely assert the empty or full state of the given
 * range of addresses. You may be wondering why they require a qthread_t
 * argument. The reason for this is memory pooling; memory is allocated on a
 * per-shepherd basis (to avoid needing to lock the memory pool). Anyway, if
 * you pass it a NULL qthread_t, it will still work. */
void qthread_empty(qthread_t * me, void *dest, const size_t bytes);
void qthread_fill(qthread_t * me, void *dest, const size_t bytes);

/* NOTE!!!!!!!!!!!
 * Reads and writes operate on machine-word-size segments of memory. That is,
 * on a 32-bit architecture, it will read/write 4 bytes at a time, and on a
 * 64-bit architecture, it will read/write 8 bytes at a time. For correct
 * operation, you will probably want to use someting like
 * __attribute__((alignment(8))) on your variables.
 */
#ifdef __LP64__
#define WORDSIZE (8)
#else
#define WORDSIZE (4)
#endif

/* This function waits for memory to become empty, and then fills it. When
 * memory becomes empty, only one thread blocked like this will be awoken. Data
 * is read from src and written to dest.
 *
 * The semantics of writeEF are:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state get changed from empty to full
 *
 * This function takes a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self()
 * (which, conveniently, returns NULL if you aren't a qthread).
 */
void qthread_writeEF(qthread_t * me, void *dest, const void *src);

/* This function waits for memory to become full, and then reads it and leaves
 * the memory as full. When memory becomes full, all threads waiting for it to
 * become full with a readFF will receive the value at once and will be queued
 * to run. Data is read from src and stored in dest.
 *
 * The semantics of readFF are:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 *
 * This function takes a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self()
 * (which, conveniently, returns NULL if you aren't a qthread).
 */
void qthread_readFF(qthread_t * me, void *dest, void *src);

/* These functions wait for memory to become full, and then empty it. When
 * memory becomes full, only one thread blocked like this will be awoken. Data
 * is read from src and written to dest.
 *
 * The semantics of readFE are:
 * 1 - src's FEB state must be "full"
 * 2 - data is copied from src to destination
 * 3 - the src's FEB bits get changed from full to empty when the data is copied
 *
 * This function takes a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self()
 * (which, conveniently, returns NULL if you aren't a qthread).
 */
void qthread_readFE(qthread_t * me, void *dest, void *src);

/* functions to implement FEB-ish locking/unlocking
 *
 * These are atomic and functional, but do not have the same semantics as full
 * FEB locking/unlocking (namely, unlocking cannot block), however because of
 * this, they have lower overhead.
 *
 * These functions take a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self().
 */
int qthread_lock(qthread_t * me, void *a);
int qthread_unlock(qthread_t * me, void *a);

#ifdef __cplusplus
}
#endif

#endif /* _QTHREAD_H_ */
