#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <pthread.h>		       /* included here only as a convenience */
#include <errno.h>		       /* for ENOMEM */

#include <qthread/qthread-int.h>       /* for uint32_t and uint64_t */
#include <qthread/common.h>	       /* important configuration options */

#ifdef SST
# include <ppcPimCalls.h>
#endif

/*****************************************************************************
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  NOTE  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! *
 *                                                                           *
 *    The most complete documentaton is going to be in the man pages. The    *
 *    documentation here is just to give you a general idea of what each     *
 *    function does.                                                         *
 *                                                                           *
 *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *****************************************************************************/

/* Return Codes */
#define QTHREAD_REDUNDANT	1
#define QTHREAD_SUCCESS		0
#define QTHREAD_BADARGS		-1
#define QTHREAD_PTHREAD_ERROR	-2
#define QTHREAD_MALLOC_ERROR	ENOMEM

#ifdef __cplusplus
extern "C"
{
#endif
#ifdef SST
typedef int qthread_t;
typedef unsigned int qthread_shepherd_id_t;
#else
typedef struct qthread_s qthread_t;
typedef unsigned char qthread_shepherd_id_t;	/* doubt we'll run more than 255 shepherds */
#endif

/* FEB locking only works on aligned addresses. On 32-bit architectures, this
 * isn't too much of an inconvenience. On 64-bit architectures, it's a pain in
 * the BUTT! This is here to try and help a little bit. */
#ifdef SUN_CC_BUG_1
# define __attribute__(x)
#endif
#ifdef __ILP64__
typedef uint64_t __attribute__ ((aligned(8))) aligned_t;
typedef int64_t __attribute__ ((aligned(8))) saligned_t;
#else
typedef uint32_t __attribute__ ((aligned(4))) aligned_t;
typedef int32_t __attribute__ ((aligned(4))) saligned_t;
#endif

/* for convenient arguments to qthread_fork */
typedef aligned_t(*qthread_f) (qthread_t * me, void *arg);

#define NO_SHEPHERD ((qthread_shepherd_id_t)-1)

/* use this function to initialize the qthreads environment before spawning any
 * qthreads. The argument to this function specifies the number of pthreads
 * that will be spawned to shepherd the qthreads. */
#ifdef SST
#define qthread_init(x) PIM_quickPrint(0xbeefee,x,PIM_readSpecial(PIM_CMD_LOC_COUNT))
#else
int qthread_init(const qthread_shepherd_id_t nshepherds);
#endif

/* use this function to clean up the qthreads environment after execution of
 * the program is finished. This function will terminate any currently running
 * qthreads, so only use it when you are certain that execution has completed.
 * For examples of how to do this, look at the included test programs. */
#ifndef SST
void qthread_finalize(void);
#else
/* XXX: not sure how to handle this in a truly multithreaded environment */
#define qthread_finalize()
#endif

/* this function allows a qthread to specifically give up control of the
 * processor even though it has not blocked. This is useful for things like
 * busy-waits or cooperative multitasking. Without this function, threads will
 * only ever allow other threads assigned to the same pthread to execute when
 * they block. */
#ifndef SST
void qthread_yield(qthread_t * me);
#else
/* means nothing in a truly multithreaded environment */
#define qthread_yield(x)
#endif

/* this function allows a qthread to retrieve its qthread_t pointer if it has
 * been lost for some reason */
#ifndef SST
qthread_t *qthread_self(void);
#else
#define qthread_self() (qthread_t*)PIM_readSpecial(PIM_CMD_THREAD_SEQ)
#endif

/* these are the functions for generating a new qthread.
 *
 * Using qthread_fork() and variants:
 *
 *     The specified function (the first argument; note that it is a qthread_f
 *     and not a qthread_t) will be run to completion. The difference between
 *     them is that a detached qthread cannot be joined, but an un-detached
 *     qthread MUST be joined (otherwise not all of its memory will be
 *     free'd). The qthread_fork_to function spawns the thread to a specific
 *     shepherd.
 */
#ifndef SST
int qthread_fork(const qthread_f f, const void *arg, aligned_t * ret);
#else
#define qthread_fork(f, arg, ret) qthread_fork_to((f), (arg), (ret), NO_SHEPHERD)
#endif
int qthread_fork_to(const qthread_f f, const void *arg, aligned_t * ret,
		    const qthread_shepherd_id_t shepherd);

/* Using qthread_prepare()/qthread_schedule() and variants:
 *
 *     The combination of these two functions works like qthread_fork().
 *     First, qthread_prepare() creates a qthread_t object that is ready to be
 *     run (almost), but has not been scheduled. Next, qthread_schedule puts
 *     the finishing touches on the qthread_t structure and places it into an
 *     active queue.
 */
#ifdef SST
#define qthread_prepare(f, arg, ret) qthread_prepare_for((f), (arg), (ret), NO_SHEPHERD)
#else
qthread_t *qthread_prepare(const qthread_f f, const void *arg,
			   aligned_t * ret);
#endif
qthread_t *qthread_prepare_for(const qthread_f f, const void *arg,
			       aligned_t * ret,
			       const qthread_shepherd_id_t shepherd);

#ifdef SST
#define qthread_schedule(t) qthread_schedule_on(t, NO_SHEPHERD)
#define qthread_schedule_on(t,shep) PIM_startStoppedThread((int)t,(int)shep)
#else
int qthread_schedule(qthread_t * t);
int qthread_schedule_on(qthread_t * t, const qthread_shepherd_id_t shepherd);
#endif

/* these are accessor functions for use by the qthreads to retrieve information
 * about themselves */
#ifdef SST
static inline unsigned qthread_id(const qthread_t * t)
{
    return PIM_readSpecial(PIM_CMD_THREAD_SEQ);
}
static inline qthread_shepherd_id_t qthread_shep(const qthread_t * t)
{
    return PIM_readSpecial(PIM_CMD_PROC_NUM);
}
static inline size_t qthread_stackleft(const qthread_t * t)
{
    return 0;			       /* XXX: this is a bug! */
}
static inline size_t qthread_retloc(const qthread_t * t)
{
    return 0;			       /* XXX: this is a bug! */
}
#else
unsigned qthread_id(const qthread_t * t);
qthread_shepherd_id_t qthread_shep(const qthread_t * t);
size_t qthread_stackleft(const qthread_t * t);
aligned_t *qthread_retloc(const qthread_t * t);
#endif

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

/* This function is just to assist with debugging; it returns 1 if the address
 * is full, and 0 if the address is empty */
#ifdef SST
static inline int qthread_feb_status(const void *addr)
{
    return PIM_feb_is_full((void *)addr);
}
#else
int qthread_feb_status(const void *addr);
#endif

/* The empty/fill functions merely assert the empty or full state of the given
 * address. You may be wondering why they require a qthread_t argument. The
 * reason for this is memory pooling; memory is allocated on a per-shepherd
 * basis (to avoid needing to lock the memory pool). Anyway, if you pass it a
 * NULL qthread_t, it will still work, it just won't be as fast. */
#ifdef SST
static inline int qthread_empty(qthread_t * me, const void *dest)
{
    PIM_feb_empty((void *)dest);
    return 0;
}
static inline int qthread_fill(qthread_t * me, const void *dest)
{
    PIM_feb_fill((void *)dest);
    return 0;
}
#else
int qthread_empty(qthread_t * me, const void *dest);
int qthread_fill(qthread_t * me, const void *dest);
#endif

/* NOTE!!!!!!!!!!!
 * Reads and writes operate on machine-word-size segments of memory. That is,
 * on a 32-bit architecture, it will read/write 4 bytes at a time, and on a
 * 64-bit architecture, it will read/write 8 bytes at a time. For correct
 * operation, you will probably want to use someting like
 * __attribute__((alignment(8))) on your variables.
 */
#ifdef __ILP64__
# ifndef WORDSIZE
#  define WORDSIZE (8)
# endif
#else
# ifndef WORDSIZE
#  define WORDSIZE (4)
# endif
#endif

/* These functions wait for memory to become empty, and then fill it. When
 * memory becomes empty, only one thread blocked like this will be awoken. Data
 * is read from src and written to dest.
 *
 * The semantics of writeEF are:
 * 1 - destination's FEB state must be "empty"
 * 2 - data is copied from src to destination
 * 3 - the destination's FEB state gets changed from empty to full
 *
 * This function takes a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self()
 * (which, conveniently, returns NULL if you aren't a qthread).
 */
#ifdef SST
static inline int qthread_writeEF(qthread_t * me, void *dest,
				  const aligned_t * src)
{
    PIM_feb_writeef(dest, *(src));
    return 0;
}
static inline int qthread_writeEF_const(qthread_t * me, void *dest,
					const aligned_t src)
{
    PIM_feb_writeef(dest, src);
    return 0;
}
#else
int qthread_writeEF(qthread_t * me, void *dest, const void *src);
int qthread_writeEF_const(qthread_t * me, void *dest, const aligned_t src);
#endif

/* This function is a cross between qthread_fill() and qthread_writeEF(). It
 * does not wait for memory to become empty, but performs the write and sets
 * the state to full atomically with respect to other FEB-based actions. Data
 * is read from src and written to dest.
 *
 * The semantics of writeF are:
 * 1 - data is copied from src to destination
 * 2 - the destination's FEB state gets set to full
 *
 * This function takes a qthread_t pointer as an argument. If this is called
 * from somewhere other than a qthread, use NULL for the me argument. If you
 * have lost your qthread_t pointer, it can be reclaimed using qthread_self()
 * (which, conveniently, returns NULL if you aren't a qthread).
 */
#ifdef SST
static inline int qthread_writeF(qthread_t * me, void *dest, const void *src)
{
    *(aligned_t *) dest = *(aligned_t *) src;
    PIM_feb_fill(dest);
    return 0;
}
static inline int qthread_writeF_const(qthread_t * me, void *dest,
				       const aligned_t src)
{
    *(aligned_t *) dest = src;
    PIM_feb_fill(dest);
    return 0;
}
#else
int qthread_writeF(qthread_t * me, void *dest, const void *src);
int qthread_writeF_const(qthread_t * me, void *dest, const aligned_t src);
#endif

/* This function waits for memory to become full, and then reads it and leaves
 * the memory as full. When memory becomes full, all threads waiting for itindent: Standard input:225: Error:Stmt nesting error.
 to
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
#ifdef SST
static inline int qthread_readFF(qthread_t * me, void * dest,
				 const void * src)
{
    if (dest != NULL && dest != src) {
	*(aligned_t*)dest = PIM_feb_readff((aligned_t *) src);
    } else {
	PIM_feb_readff((aligned_t *) src);
    }
    return 0;
}
#else
int qthread_readFF(qthread_t * me, void *dest, const void *src);
#endif

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
#ifdef SST
static inline int qthread_readFE(qthread_t * me, void *dest, void *src)
{
    if (dest != NULL && dest != src) {
	*(aligned_t *) dest = PIM_feb_readfe(src);
    } else {
	PIM_feb_readfe(src);
    }
    return 0;
}
#else
int qthread_readFE(qthread_t * me, void *dest, void *src);
#endif

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
#ifdef SST
static inline int qthread_lock(qthread_t * me, const void *a)
{
    PIM_feb_readfe((aligned_t *) a);
    return 0;
}
static inline int qthread_unlock(qthread_t * me, const void *a)
{
    PIM_feb_fill((aligned_t *) a);
    return 0;
}
#else
int qthread_lock(qthread_t * me, const void *a);
int qthread_unlock(qthread_t * me, const void *a);
#endif

/* this implements an atomic increment. It is done with architecture-specific
 * assembly and does NOT use FEB's or lock/unlock (except in the slow fallback
 * for unrecognized architectures)... but usually that's not the issue.
 * It returns the value of the contents of operand before incrementing. */
#ifdef SST
static inline aligned_t qthread_incr(aligned_t * operand, const int incr)
{
    return PIM_atomicIncrement(operand, incr);
}
#else
static inline aligned_t qthread_incr(aligned_t * operand, const int incr)
{				       /*{{{ */
    aligned_t retval;

#if !defined(QTHREAD_MUTEX_INCREMENT) && ( __PPC__ || _ARCH_PPC || __powerpc__ )
    register unsigned int incrd = incrd;	/* no initializing */
    asm volatile ("1:\tlwarx  %0,0,%1\n\t" "add    %3,%0,%2\n\t" "stwcx. %3,0,%1\n\t" "bne-   1b\n\t"	/* if it failed, try again */
		  "isync"	/* make sure it wasn't all a dream */
		  :"=&b"   (retval)
		  :"r"     (operand), "r"(incr), "r"(incrd)
		  :"cc", "memory");
/*#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( __sun__ )
    register unsigned int incrd = incrd; // no initializing
    __asm__ __volatile__ (
	    "1: lduw   [%1], %0\n\t"
	    "add    %0, %3, %2\n\t"
	    "cas    [%1], %0, %2\n\t"
	    "cmp    %0, %2\n\t"
	    "bne,pn %icc, 1b\n\t"
	    :"=&r" (retval)
	    :"r" (operand), "r" (incrd), "r" (incr)
	    :"icc", "memory");*/
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ! defined(__INTEL_COMPILER) && ( __ia64 || __ia64__ )
# ifdef __ILP64__
    int64_t res;

    if (incr == 1) {
	asm volatile ("fetchadd8.rel %0=%1,1":"=r" (res)
		      :"m"     (*operand));

	retval = res;
    } else {
	int64_t old, newval;

	do {
	    old = *operand;	       /* atomic, because operand is aligned */
	    newval = old + incr;
	    asm volatile ("mov ar.ccv=%0;;":	/* no output */
			  :"rO"    (old));

	    /* separate so the compiler can insert its junk */
	    asm volatile ("cmpxchg8.acq %0=[%1],%2,ar.ccv":"=r" (res)
			  :"r"     (operand), "r"(newval)
			  :"memory");
	} while (res != old);	       /* if res==old, the calc is out of date */
	retval = old;
    }
# else
    int32_t res;

    if (incr == 1) {
	asm volatile ("fetchadd4.rel %0=%1,1":"=r" (res)
		      :"m"     (*operand));

	retval = res;
    } else {
	int32_t old, newval;

	do {
	    old = *operand;	       /* atomic, because operand is aligned */
	    newval = old + incr;
	    asm volatile ("mov ar.ccv=%0;;":	/* no output */
			  :"rO"    (old));

	    /* separate so the compiler can insert its junk */
	    asm volatile ("cmpxchg4.acq %0=[%1],%2,ar.ccv":"=r" (res)
			  :"r"     (operand), "r"(newval)
			  :"memory");
	} while (res != old);	       /* if res==old, the calc is out of date */
	retval = old;
    }
# endif
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( __x86_64 || __x86_64__ )
    retval = incr;
    asm volatile ("lock xaddl %0, %1;":"=r" (retval)
		  :"m"     (*operand), "0"(retval));
#elif !defined(QTHREAD_MUTEX_INCREMENT) && ( __i486 || __i486__ )
    retval = incr;
    asm volatile (".section .smp_locks,\"a\"\n" "  .align 4\n" "  .long 661f\n" ".previous\n" "661:\n"	/* the preceeding gobbledygook is stolen from the Linux kernel */
		  "lock xaddl %1,%0"	/* atomically add incr to operand */
		  :		/* no output */
		  :"m"     (*operand), "ir"(retval));
#else
#ifndef QTHREAD_MUTEX_INCREMENT
#warning unrecognized architecture: falling back to safe but very slow increment implementation
#endif
    qthread_t *me = qthread_self();

    qthread_lock(me, operand);
    *operand += incr;
    retval = *operand;
    qthread_unlock(me, operand);
#endif
    return retval;
}				       /*}}} */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _QTHREAD_H_ */
