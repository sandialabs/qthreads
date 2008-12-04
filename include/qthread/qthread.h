#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <errno.h>		       /* for ENOMEM */

#include <qthread/qthread-int.h>       /* for uint32_t and uint64_t */
#include <qthread/common.h>	       /* important configuration options */

#include <string.h>		       /* for memcpy() */

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
#define NO_SHEPHERD ((qthread_shepherd_id_t)-1)

/* NOTE!!!!!!!!!!!
 * Reads and writes operate on aligned_t-size segments of memory.
 *
 * FEB locking only works on aligned addresses. On 32-bit architectures, this
 * isn't too much of an inconvenience. On 64-bit architectures, it's a pain in
 * the BUTT! This is here to try and help a little bit. */
#ifndef HAVE_ATTRIBUTE_ALIGNED
# define __attribute__(x)
#endif

#ifdef __cplusplus
extern "C"
{
#endif
#if QTHREAD_SIZEOF_ALIGNED_T == 4
typedef uint32_t __attribute__ ((aligned(QTHREAD_ALIGNMENT_ALIGNED_T))) aligned_t;
typedef int32_t __attribute__ ((aligned(QTHREAD_ALIGNMENT_ALIGNED_T))) saligned_t;
#elif QTHREAD_SIZEOF_ALIGNED_T == 8
typedef uint64_t __attribute__ ((aligned(QTHREAD_ALIGNMENT_ALIGNED_T))) aligned_t;
typedef int64_t __attribute__ ((aligned(QTHREAD_ALIGNMENT_ALIGNED_T))) saligned_t;
#else
#error "Don't know type for sizeof aligned_t"
#endif
#ifdef __cplusplus
}
#endif

#ifdef SST
# include <qthread/qthread-sst.h>
#else
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct qthread_s qthread_t;
typedef unsigned short qthread_shepherd_id_t;	/* doubt we'll run more than 65k shepherds */

/* for convenient arguments to qthread_fork */
typedef aligned_t(*qthread_f) (qthread_t * me, void *arg);

/* use this function to initialize the qthreads environment before spawning any
 * qthreads. The argument to this function specifies the number of pthreads
 * that will be spawned to shepherd the qthreads. */
int qthread_init(const qthread_shepherd_id_t nshepherds);

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

/* this function allows a qthread to retrieve its qthread_t pointer if it has
 * been lost for some reason */
qthread_t *qthread_self(void);

/* these are the functions for generating a new qthread.
 *
 * Using qthread_fork() and variants:
 *
 *     The specified function (the first argument; note that it is a qthread_f
 *     and not a qthread_t) will be run to completion. You can detect that a
 *     thread has finished by specifying a location to store the return value
 *     (which will be stored with a qthread_writeF call). The qthread_fork_to
 *     function spawns the thread to a specific shepherd.
 */
int qthread_fork(const qthread_f f, const void * const arg, aligned_t * ret);
int qthread_fork_to(const qthread_f f, const void * const arg, aligned_t * ret,
		    const qthread_shepherd_id_t shepherd);

/* Using qthread_prepare()/qthread_schedule() and variants:
 *
 *     The combination of these two functions works like qthread_fork().
 *     First, qthread_prepare() creates a qthread_t object that is ready to be
 *     run (almost), but has not been scheduled. Next, qthread_schedule puts
 *     the finishing touches on the qthread_t structure and places it into an
 *     active queue.
 */
qthread_t *qthread_prepare(const qthread_f f, const void * const arg,
			   aligned_t * ret);
qthread_t *qthread_prepare_for(const qthread_f f, const void * const arg,
			       aligned_t * ret,
			       const qthread_shepherd_id_t shepherd);

int qthread_schedule(qthread_t * t);
int qthread_schedule_on(qthread_t * t, const qthread_shepherd_id_t shepherd);

/* these are accessor functions for use by the qthreads to retrieve information
 * about themselves */
unsigned qthread_id(const qthread_t * t);
qthread_shepherd_id_t qthread_shep(const qthread_t * t);
size_t qthread_stackleft(const qthread_t * t);
aligned_t *qthread_retloc(const qthread_t * t);

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
int qthread_feb_status(const aligned_t *addr);

/* The empty/fill functions merely assert the empty or full state of the given
 * address. You may be wondering why they require a qthread_t argument. The
 * reason for this is memory pooling; memory is allocated on a per-shepherd
 * basis (to avoid needing to lock the memory pool). Anyway, if you pass it a
 * NULL qthread_t, it will still work, it just won't be as fast. */
int qthread_empty(qthread_t * me, const aligned_t *dest);
int qthread_fill(qthread_t * me, const aligned_t *dest);

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
int qthread_writeEF(qthread_t * me, void *dest, const void *src);
int qthread_writeEF_const(qthread_t * me, void *dest, const aligned_t src);

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
int qthread_writeF(qthread_t * me, void *dest, const void *src);
int qthread_writeF_const(qthread_t * me, void *dest, const aligned_t src);

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
int qthread_readFF(qthread_t * me, aligned_t * const dest, const aligned_t * const src);

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
int qthread_readFE(qthread_t * me, void *dest, void *src);

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
int qthread_lock(qthread_t * me, const aligned_t *a);
int qthread_unlock(qthread_t * me, const aligned_t *a);

/* the following three functions implement variations on atomic increment. It
 * is done with architecture-specific assembly (on supported architectures,
 * when possible) and does NOT use FEB's or lock/unlock unless the architecture
 * is unsupported or cannot perform atomic operations at the right granularity.
 * All of these functions return the value of the contents of the operand
 * *after* incrementing.
 */

static inline float qthread_fincr(volatile float * operand, const float incr)
{/*{{{*/
#if defined(HAVE_GCC_INLINE_ASSEMBLY)
# if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    union {
	float f;
	uint32_t i;
    } retval;
    register float incrd = incrd;
    uint32_t scratch;
    __asm__ __volatile__ ("1:\n\t"
	    "lwarx  %0,0,%3\n\t"
	    // convert from int to float
	    "stw    %0,%2\n\t"
	    "lfs    %1,%2\n\t"
	    // do the addition
	    "fadds  %1,%1,%4\n\t"
	    // convert from float to int
	    "stfs   %1,%2\n\t"
	    "lwz    %0,%2\n\t"
	    // store back to original location
	    "stwcx. %0,0,%3\n\t"
	    "bne-   1b\n\t"
	    "isync"
	    :"=&b" (retval.i), "=&f"(incrd), "=m"(scratch)
	    :"r" (operand), "f"(incr)
	    :"cc","memory");
    return retval.f-incr;
# elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    union {
	float f;
	uint32_t i;
    } oldval, newval;
    /* newval.f = *operand; */
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
	oldval.f = *operand;
	newval.f = oldval.f + incr;
	__asm__ __volatile__ ("cas [%1], %2, %0"
			      : "=&r" (newval.i)
			      : "r" (operand), "r"(oldval.i), "0" (newval.i)
			      : "cc", "memory");
    } while (oldval.i != newval.i);
    return oldval.f;
# elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    union {
	float f;
	uint32_t i;
    } oldval, newval, res;
    do {
	oldval.f = *operand;
	newval.f = oldval.f + incr;
	__asm__ __volatile__ ("mov ar.ccv=%0;;": :"rO" (oldval.i));
	__asm__ __volatile__ ("cmpxchg4.acq %0=[%1],%2,ar.ccv"
		:"=r" (res.i)
		:"r"(operand), "r"(newval.i)
		:"memory");
    } while (res.i != oldval.i); /* if res!=old, the calc is out of date */
    return oldval.f;
# elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
    union {
	float f;
	uint32_t i;
    } oldval, newval, retval;
    do {
	oldval.f = *operand;
	newval.f = oldval.f + incr;
	__asm__ __volatile__ ("lock; cmpxchg %1, %2"
		:"=a"(retval.i) /* store from EAX */
		:"r"(newval.i), "m"(*(uint64_t*)operand),
		    "0"(oldval.i) /* load into EAX */
		:"cc","memory");
    } while (retval.i != oldval.i);
    return oldval.f;
# endif
#elif defined (QTHREAD_MUTEX_INCREMENT)

    float retval;
    qthread_t *me = qthread_self();
    qthread_lock(me, (aligned_t *)operand);
    retval = *operand;
    *operand += incr;
    qthread_unlock(me, (aligned_t *)operand);
    return retval;
#else
#error "Neither atomic nor mutex increment enabled; needed for qthread_fincr"
#endif
}/*}}}*/

static inline double qthread_dincr(volatile double * operand, const double incr)
{/*{{{*/
#if defined(HAVE_GCC_INLINE_ASSEMBLY) && (QTHREAD_ASSEMBLY_ARCH != QTHREAD_POWERPC32)
#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    register uint64_t incrd = incrd;
    register double incrf = incrf;
    union {
	uint64_t i;
	double d;
    } retval;
    uint64_t scratch = scratch;
    __asm__ __volatile__ ("1:\n\t"
	    "ldarx %0,0,%1\n\t"
	    /*convert from integer to floating point*/
	    "std   %0,%3\n\t" // %3 is scratch memory (NOT a register)
	    "lfd   %4,%3\n\t" // %4 is a scratch floating point register
	    /* do the addition */
	    "fadd  %4,%5,%4\n\t" // %5 is the increment floating point register
	    /* convert from floating point to integer */
	    "stfd   %4,%3\n\t"
	    "ld     %2,%3\n\t"
	    /* store back to original location */
	    "stdcx. %2,0,%1\n\t"
	    "bne-   1b\n\t"
	    "isync"
	    :"=&b" (retval.i)
	    :"r"(operand),"r"(incrd),"m"(scratch),"f"(incrf),"f"(incr)
	    :"cc","memory");
    return retval.d;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    double oldval, newval;
    newval = *operand;
    do {
	/* this allows the compiler to be as flexible as possible with register
	 * assignments */
	register uint64_t tmp1=tmp1;
	register uint64_t tmp2=tmp2;
	oldval = newval;
        newval = oldval + incr;
        __asm__ __volatile__ (
		"ldx %0, %1\n\t"
		"ldx %4, %2\n\t"
		"casx [%3], %2, %1\n\t"
		"stx %1, %0"
		/* h means 64-BIT REGISTER
		 * (probably unnecessary, but why take chances?) */
			      : "=m" (newval), "=&h"(tmp1), "=&h"(tmp2)
                              : "r" (operand), "m"(oldval)
                              : "memory");
    } while (oldval != newval);
    return oldval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    union {
	uint64_t i;
	double d;
    } oldval, newval;
    /*newval.d = *operand;*/
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
        oldval.d = *operand;
        newval.d = oldval.d + incr;
        __asm__ __volatile__ ("casx [%1], %2, %0"
                              : "=&r" (newval.i)
                              : "r" (operand), "r"(oldval.i), "0" (newval.i)
                              : "memory");
    } while (oldval.d != newval.d);
    return oldval.d;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    union {
	uint64_t i;
	double d;
    } oldval, newval, res;
    do {
	oldval.d = *operand;
	newval.d = oldval.d + incr;
	__asm__ __volatile__ ("mov ar.ccv=%0;;": :"rO" (oldval.i));
	__asm__ __volatile__ ("cmpxchg8.acq %0=[%1],%2,ar.ccv"
		:"=r"(res.i)
		:"r"(operand), "r"(newval.i)
		:"memory");
    } while (res.i != oldval.i); /* if res!=old, the calc is out of date */
    return oldval.d;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
    union {
	double d;
	uint64_t i;
    } oldval, newval, retval;
    do {
	oldval.d = *operand;
	newval.d = oldval.d + incr;
	__asm__ __volatile__ ("lock; cmpxchgq %1, %2":"=a"(retval.i)
		:"r"(newval.i), "m"(*(uint64_t*)operand), "0"(oldval.i)
		:"memory");
    } while (retval.i != oldval.i);
    return oldval.d;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
    union {
	double d;
	uint64_t i;
	struct {
	    /* note: the ordering of these is both important and
	     * counter-intuitive; welcome to little-endian! */
	    uint32_t l;
	    uint32_t h;
	} s;
    } oldval, newval;
    register char test;
    do {
#ifdef __PIC__
	/* this saves off %ebx to make PIC code happy :P */
# define QTHREAD_PIC_PREFIX "pushl %%ebx\n\tmovl %4, %%ebx\n\t"
	/* this restores it */
# define QTHREAD_PIC_SUFFIX "\n\tpopl %%ebx"
# define QTHREAD_PIC_REG "m"
#else
# define QTHREAD_PIC_PREFIX
# define QTHREAD_PIC_SUFFIX
# define QTHREAD_PIC_REG "b"
#endif
	oldval.d = *operand;
	newval.d = oldval.d + incr;
	/* Yeah, this is weird looking, but it really makes sense when you
	 * understand the instruction's semantics (which make sense when you
	 * consider that it's doing a 64-bit op on a 32-bit proc):
	 *
	 *    Compares the 64-bit value in EDX:EAX with the operand
	 *    (destination operand). If the values are equal, the 64-bit value
	 *    in ECX:EBX is stored in the destination operand. Otherwise, the
	 *    value in the destination operand is loaded into EDX:EAX."
	 *
	 * So what happens is the oldval is loaded into EDX:EAX and the newval
	 * is loaded into ECX:EBX to start with (i.e. as inputs). Then
	 * CMPXCHG8B does its business, after which EDX:EAX is guaranteed to
	 * contain the value of *operand when the instruction executed. We test
	 * the ZF field to see if the operation succeeded. We *COULD* save
	 * EDX:EAX back into oldval to save ourselves a step when the loop
	 * fails, but that's a waste when the loop succeeds (i.e. in the common
	 * case). Optimizing for the common case, in this situation, means
	 * minimizing our extra write-out to the one-byte test variable.
	 */
	__asm__ __volatile__ (
		QTHREAD_PIC_PREFIX
		"lock; cmpxchg8b %1\n\t"
		"setne %0" /* test = (ZF==0) */
		QTHREAD_PIC_SUFFIX
		:"=r"(test)
		:"m"(*operand),
		/*EAX*/"a"(oldval.s.l),
		/*EDX*/"d"(oldval.s.h),
		/*EBX*/QTHREAD_PIC_REG(newval.s.l),
		/*ECX*/"c"(newval.s.h)
		:"memory");
    } while (test); /* if ZF was cleared, the calculation is out of date */
    return oldval.d;

#else
#error "Unimplemented assembly architecture"
#endif
#elif defined (QTHREAD_MUTEX_INCREMENT) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32)

    double retval;
    qthread_t *me = qthread_self();
    qthread_lock(me, (aligned_t*)operand);
    retval = *operand;
    *operand += incr;
    qthread_unlock(me, (aligned_t*)operand);
    return retval;
#else
#error "Neither atomic nor mutex increment enabled; needed for qthread_dincr"
#endif
}/*}}}*/

static inline uint32_t qthread_incr32(volatile uint32_t * operand, const int incr)
{				       /*{{{ */
#if defined(HAVE_GCC_INLINE_ASSEMBLY)

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    uint32_t retval;
    register unsigned int incrd = incrd;	/* no initializing */
    asm volatile (
	    "1:\tlwarx  %0,0,%1\n\t"
	    "add    %3,%0,%2\n\t"
	    "stwcx. %3,0,%1\n\t"
	    "bne-   1b\n\t"	/* if it failed, try again */
	    "isync"	/* make sure it wasn't all a dream */
	    :"=&b"   (retval)
	    :"r"     (operand), "r"(incr), "r"(incrd)
	    :"cc", "memory");
    return retval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    register uint32_t oldval, newval;
    /* newval = *operand; */
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
	oldval = *operand;
        newval = oldval + incr;
	/* newval always gets the value of *operand; if it's
	 * the same as oldval, then the swap was successful */
        __asm__ __volatile__ ("cas [%1] , %2, %0"
                              : "=&r" (newval)
                              : "r" (operand), "r"(oldval), "0"(newval)
                              : "cc", "memory");
    } while (oldval != newval);
    return oldval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    uint32_t res;

    if (incr == 1) {
	asm volatile ("fetchadd4.rel %0=[%1],1":"=r" (res)
		      :"r"     (operand));
    } else {
	uint32_t old, newval;

	do {
	    old = *operand;	       /* atomic, because operand is aligned */
	    newval = old + incr;
	    asm volatile ("mov ar.ccv=%0;;":	/* no output */
			  :"rO"    (old));

	    /* separate so the compiler can insert its junk */
	    asm volatile ("cmpxchg4.acq %0=[%1],%2,ar.ccv":"=r" (res)
			  :"r"     (operand), "r"(newval)
			  :"memory");
	} while (res != old);	       /* if res!=old, the calc is out of date */
    }
    return res;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)

    uint32_t retval = incr;
    asm volatile ("lock ;  xaddl %0, %1;"
		  :"=r"(retval)
		  :"m"(*operand), "0"(retval)
		  : "memory");
    return retval;
#else

#error "Unimplemented assembly architecture"

#endif

#elif defined(QTHREAD_MUTEX_INCREMENT)
    uint32_t retval;
    qthread_t *me = qthread_self();

    qthread_lock(me, (aligned_t *)operand);
    retval = *operand;
    *operand += incr;
    qthread_unlock(me, (aligned_t *)operand);
    return retval;
#else

#error "Architecture unsupported for 32-bit atomic ops, and FEB increment not enabled"

#endif
}				       /*}}} */

static inline uint64_t qthread_incr64(volatile uint64_t * operand, const int incr)
{				       /*{{{ */
#if defined(HAVE_GCC_INLINE_ASSEMBLY)

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    uint64_t retval;
    register uint64_t incrd = incrd;	/* no initializing */

    asm volatile (
	    "1:\tldarx  %0,0,%1\n\t"
	    "add    %3,%0,%2\n\t"
	    "stdcx. %3,0,%1\n\t"
	    "bne-   1b\n\t"	/* if it failed, try again */
	    "isync"	/* make sure it wasn't all a dream */
	    :"=&b"   (retval)
	    :"r"     (operand), "r"(incr), "r"(incrd)
	    :"cc", "memory");
    return retval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    uint64_t oldval, newval = *operand;

    do {
	/* this allows the compiler to be as flexible as possible with register
	 * assignments */
	register uint64_t tmp1=tmp1;
	register uint64_t tmp2=tmp2;
	oldval = newval;
        newval += incr;
	/* newval always gets the value of *operand; if it's
	 * the same as oldval, then the swap was successful */
        __asm__ __volatile__ (
		"ldx %0, %1\n\t"
		"ldx %4, %2\n\t"
		"casx [%3] , %2, %1\n\t"
		"stx %1, %0"
		/* h means 64-BIT REGISTER
		 * (probably unnecessary, but why take chances?) */
                              : "=m" (newval), "=&h"(tmp1), "=&h"(tmp2)
                              : "r" (operand), "m"(oldval)
                              : "cc", "memory");
    } while (oldval != newval);
    return oldval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    register uint64_t oldval, newval;

    /* newval = *operand; */
    do {
	/* you *should* be able to move the *operand reference outside the
	 * loop and use the output of the CAS (namely, newval) instead.
	 * However, there seems to be a bug in gcc 4.0.4 wherein, if you do
	 * that, the while() comparison uses a temporary register value for
	 * newval that has nothing to do with the output of the CAS
	 * instruction. (See how obviously wrong that is?) For some reason that
	 * I haven't been able to figure out, moving the *operand reference
	 * inside the loop fixes that problem, even at -O2 optimization. */
	oldval = *operand;
        newval = oldval + incr;
	/* newval always gets the value of *operand; if it's
	 * the same as oldval, then the swap was successful */
        __asm__ __volatile__ ("casx [%1] , %2, %0"
                              : "=&r" (newval)
                              : "r" (operand), "r"(oldval), "0"(newval)
                              : "cc", "memory");
    } while (oldval != newval);
    return oldval;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    uint64_t res;

    if (incr == 1) {
	asm volatile ("fetchadd8.rel %0=%1,1":"=r" (res)
		      :"m"     (*operand));
    } else {
	uint64_t old, newval;

	do {
	    old = *operand;	       /* atomic, because operand is aligned */
	    newval = old + incr;
	    asm volatile ("mov ar.ccv=%0;;":	/* no output */
			  :"rO"    (old));

	    /* separate so the compiler can insert its junk */
	    asm volatile ("cmpxchg8.acq %0=[%1],%2,ar.ccv":"=r" (res)
			  :"r"     (operand), "r"(newval)
			  :"memory");
	} while (res != old);	       /* if res!=old, the calc is out of date */
    }
    return res;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
    union {
	uint64_t i;
	struct {
	    /* note: the ordering of these is both important and
	     * counter-intuitive; welcome to little-endian! */
	    uint32_t l;
	    uint32_t h;
	} s;
    } oldval, newval;
    register char test;

    do {
#ifdef __PIC__
	/* should share this code with the dincr stuff */
	/* this saves off %ebx to make PIC code happy :P */
# define QTHREAD_PIC_PREFIX "pushl %%ebx\n\tmovl %4, %%ebx\n\t"
	/* this restores it */
# define QTHREAD_PIC_SUFFIX "\n\tpopl %%ebx"
# define QTHREAD_PIC_REG "m"
#else
# define QTHREAD_PIC_PREFIX
# define QTHREAD_PIC_SUFFIX
# define QTHREAD_PIC_REG "b"
#endif
	oldval.i = *operand;
	newval.i = oldval.i + incr;
	/* Yeah, this is weird looking, but it really makes sense when you
	 * understand the instruction's semantics (which make sense when you
	 * consider that it's doing a 64-bit op on a 32-bit proc):
	 *
	 *    Compares the 64-bit value in EDX:EAX with the operand
	 *    (destination operand). If the values are equal, the 64-bit value
	 *    in ECX:EBX is stored in the destination operand. Otherwise, the
	 *    value in the destination operand is loaded into EDX:EAX."
	 *
	 * So what happens is the oldval is loaded into EDX:EAX and the newval
	 * is loaded into ECX:EBX to start with (i.e. as inputs). Then
	 * CMPXCHG8B does its business, after which EDX:EAX is guaranteed to
	 * contain the value of *operand when the instruction executed. We test
	 * the ZF field to see if the operation succeeded. We *COULD* save
	 * EDX:EAX back into oldval to save ourselves a step when the loop
	 * fails, but that's a waste when the loop succeeds (i.e. in the common
	 * case). Optimizing for the common case, in this situation, means
	 * minimizing our extra write-out to the one-byte test variable.
	 */
	__asm__ __volatile__ (
		QTHREAD_PIC_PREFIX
		"lock; cmpxchg8b %1\n\t"
		"setne %0" /* test = (ZF==0) */
		QTHREAD_PIC_SUFFIX
		:"=r"(test)
		:"m"(*operand),
		/*EAX*/"a"(oldval.s.l),
		/*EDX*/"d"(oldval.s.h),
		/*EBX*/QTHREAD_PIC_REG(newval.s.l),
		/*ECX*/"c"(newval.s.h)
		:"memory");
    } while (test); /* if ZF was cleared, the calculation is out of date */
    return oldval.i;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
    uint64_t retval = incr;

    asm volatile ("lock ; xaddq %0, %1;"
		  :"=r"(retval)
		  :"m"(*operand), "0"(retval)
		  : "memory");

    return retval;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)

    /* In general, RISC doesn't provide a way to do 64 bit
    operations from 32 bit code.  Sorry. */
    uint64_t retval;
    qthread_t *me = qthread_self();

    qthread_lock(me, (aligned_t *)operand);
    retval = *operand;
    *operand += incr;
    qthread_unlock(me, (aligned_t *)operand);
    return retval;

#else

#error "Unimplemented assembly architecture"

#endif

#elif defined(QTHREAD_MUTEX_INCREMENT)

    uint64_t retval;
    qthread_t *me = qthread_self();

    qthread_lock(me, (aligned_t *)operand);
    retval = *operand;
    *operand += incr;
    qthread_unlock(me, (aligned_t *)operand);
    return retval;

#else

#error "Architecture unsupported for 64-bit atomic ops, and FEB increment not enabled"

#endif
}				       /*}}} */

#define qthread_incr( ADDR, INCVAL )                  \
   qthread_incr_xx( (volatile void*)(ADDR), (int)(INCVAL), sizeof(*(ADDR)) )

static inline unsigned long
qthread_incr_xx(volatile void* addr, int incr, size_t length)
{
    switch( length ) {
    case 4:
        return qthread_incr32((volatile uint32_t*) addr, incr);
   case 8:
        return qthread_incr64((volatile uint64_t*) addr, incr);
   default:
      /* This should never happen, so deliberately cause a seg fault
         for corefile analysis */
      *(int*)(0) = 0;
   }
   return 0;  /* compiler check */
}


#ifdef __cplusplus
}
#endif

#endif

#endif /* _QTHREAD_H_ */
