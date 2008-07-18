#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <pthread.h>		       /* included here only as a convenience */
#include <errno.h>		       /* for ENOMEM */

#include <qthread/qthread-int.h>       /* for uint32_t and uint64_t */
#include <qthread/common.h>	       /* important configuration options */

#include <string.h>		       /* for memcpy() */

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

#ifdef __cplusplus
extern "C"
{
#endif
#ifdef SST
typedef int qthread_t;
typedef unsigned int qthread_shepherd_id_t;
#else
typedef struct qthread_s qthread_t;
typedef unsigned short qthread_shepherd_id_t;	/* doubt we'll run more than 65k shepherds */
#endif

/* Return Codes */
#define QTHREAD_REDUNDANT	1
#define QTHREAD_SUCCESS		0
#define QTHREAD_BADARGS		-1
#define QTHREAD_PTHREAD_ERROR	-2
#define QTHREAD_MALLOC_ERROR	ENOMEM
#define NO_SHEPHERD ((qthread_shepherd_id_t)-1)

/* NOTE!!!!!!!!!!!
 * Reads and writes operate on aligned_t-size segments of memory. That is,
 * it will read/write 4 bytes at a time, unless you've configured it to use a
 * 64-bit aligned_t so that it will read/write 8 bytes at a time.
 *
 * FEB locking only works on aligned addresses. On 32-bit architectures, this
 * isn't too much of an inconvenience. On 64-bit architectures, it's a pain in
 * the BUTT! This is here to try and help a little bit. */
#ifndef HAVE_ATTRIBUTE_ALIGNED
# define __attribute__(x)
#endif

#ifdef QTHREAD_64_BIT_ALIGN_T
typedef uint64_t __attribute__ ((aligned(8))) aligned_t;
typedef int64_t __attribute__ ((aligned(8))) saligned_t;
#else
#ifdef QTHREAD_64_BIT_ALIGN
typedef uint32_t __attribute__ ((aligned(8))) aligned_t;
typedef int32_t __attribute__ ((aligned(8))) saligned_t;
#else
typedef uint32_t __attribute__ ((aligned(4))) aligned_t;
typedef int32_t __attribute__ ((aligned(4))) saligned_t;
#endif
#endif

/* for convenient arguments to qthread_fork */
typedef aligned_t(*qthread_f) (qthread_t * me, void *arg);

/* use this function to initialize the qthreads environment before spawning any
 * qthreads. The argument to this function specifies the number of pthreads
 * that will be spawned to shepherd the qthreads. */
#ifdef SST
#define qthread_init(x) PIM_quickPrint(0x5ca1ab1e,x,PIM_readSpecial(PIM_CMD_LOC_COUNT))
#else
int qthread_init(const qthread_shepherd_id_t nshepherds);
#endif

/* use this function to clean up the qthreads environment after execution of
 * the program is finished. This function will terminate any currently running
 * qthreads, so only use it when you are certain that execution has completed.
 * For examples of how to do this, look at the included test programs. */
#ifdef SST
/* XXX: not sure how to handle this in a truly multithreaded environment */
#define qthread_finalize()
#else
void qthread_finalize(void);
#endif

/* this function allows a qthread to specifically give up control of the
 * processor even though it has not blocked. This is useful for things like
 * busy-waits or cooperative multitasking. Without this function, threads will
 * only ever allow other threads assigned to the same pthread to execute when
 * they block. */
#ifdef SST
/* means nothing in a truly multithreaded environment */
#define qthread_yield(x)
#else
void qthread_yield(qthread_t * me);
#endif

/* this function allows a qthread to retrieve its qthread_t pointer if it has
 * been lost for some reason */
#ifdef SST
#define qthread_self() (qthread_t*)PIM_readSpecial(PIM_CMD_THREAD_SEQ)
#else
qthread_t *qthread_self(void);
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
#ifdef SST
#define qthread_fork(f, arg, ret) qthread_fork_to((f), (arg), (ret), NO_SHEPHERD)
#else
int qthread_fork(const qthread_f f, const void *arg, aligned_t * ret);
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
    return PIM_feb_is_full((unsigned int*)addr);
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
    PIM_feb_empty((unsigned int*)dest);
    return 0;
}
static inline int qthread_fill(qthread_t * me, const void *dest)
{
    PIM_feb_fill((unsigned int*)dest);
    return 0;
}
#else
int qthread_empty(qthread_t * me, const void *dest);
int qthread_fill(qthread_t * me, const void *dest);
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
				  const void * src)
{
    PIM_feb_writeef((unsigned int*)dest, *(aligned_t *)src);
    return 0;
}
static inline int qthread_writeEF_const(qthread_t * me, void *dest,
					const aligned_t src)
{
    PIM_feb_writeef((unsigned int*)dest, src);
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
    PIM_feb_fill((unsigned int*)dest);
    return 0;
}
static inline int qthread_writeF_const(qthread_t * me, void *dest,
				       const aligned_t src)
{
    *(aligned_t *) dest = src;
    PIM_feb_fill((unsigned int*)dest);
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
	*(aligned_t *) dest = PIM_feb_readfe((unsigned int*)src);
    } else {
	PIM_feb_readfe((unsigned int*)src);
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

/* the following three functions implement variations on atomic increment. It
 * is done with architecture-specific assembly (on supported architectures,
 * when possible) and does NOT use FEB's or lock/unlock unless the architecture
 * is unsupported or cannot perform atomic operations at the right granularity.
 * All of these functions return the value of the contents of the operand
 * *after* incrementing.
 */

static inline float qthread_fincr(volatile float * operand, const float incr)
{
#if defined(HAVE_GCC_INLINE_ASSEMBLY)
# if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    union {
	float f;
	uint32_t i;
    } retval;
    register float incrd = incrd;
    uint32_t scratch;
    __asm__ __volatile__ ("1:\n\t"
	    "lwarx  %0,0,%1\n\t"
	    // convert from int to float
	    "stw    %0,%4\n\t"
	    "lfs    %3,%4\n\t"
	    // do the addition
	    "fadds  %3,%3,%2\n\t"
	    // convert from float to int
	    "stfs   %3,%4\n\t"
	    "lwz    %0,%4\n\t"
	    // store back to original location
	    "stwcx. %0,0,%1\n\t"
	    "bne-   1b\n\t"
	    "isync"
	    :"=&b" (retval.i)
	    :"r" (operand), "f"(incr), "f"(incrd), "m"(scratch)
	    :"cc","memory");
    return retval.f;
# elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    union {
	float f;
	uint32_t i;
    } oldval, newval;
    newval.f = *operand;
    do {
	oldval = newval;
	newval.f = oldval.f + incr;
	__asm__ __volatile__ ("cas [%1], %2, %0"
			      : "+r" (newval.i)
			      : "r" (operand), "r"(oldval.i)
			      : "cc", "memory");
    } while (oldval.i != newval.i);
    return oldval.f + incr;
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
    return res.d+incr;
# endif
#elif defined (QTHREAD_MUTEX_INCREMENT)

    float retval;
    qthread_t me = qthread_self();
    qthread_lock(me, (void*)operand);
    retval = *operand += incr;
    qthread_unlock(me, (void*)operand);
    return retval;
#else
#error "Neither atomic nor mutex increment enabled; needed for qthread_fincr"
#endif
}

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
    return retval.d + incr;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    union {
	uint64_t i;
	double d;
    } oldval, newval;
    newval.d = *operand;
    do {
        oldval = newval;
        newval.d = oldval.d + incr;
        __asm__ __volatile__ ("casx [%1], %2, %0"
                              : "+r" (newval.i)
                              : "r" (operand), "r"(oldval.i)
                              : "cc", "memory");
    } while (oldval.i != newval.i);
    return oldval.d + incr;
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
    return res.d+incr;

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
    return retval.d;

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
    return newval.d;

#else
#error "Unimplemented assembly architecture"
#endif
#elif defined (QTHREAD_MUTEX_INCREMENT) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32)

    double retval;
    qthread_lock(qthread_self(), (void*)operand);
    retval = *operand += incr;
    qthread_unlock(qthread_self(), (void*)operand);
    return retval;
#else
#error "Neither atomic nor mutex increment enabled; needed for qthread_dincr"
#endif
}/*}}}*/

#ifdef SST
static inline aligned_t qthread_incr(volatile aligned_t * operand, const int incr)
{
    return PIM_atomicIncrement(operand, incr) + incr;
}

#else

static inline aligned_t qthread_incr(volatile aligned_t * operand, const int incr)
{				       /*{{{ */
    aligned_t retval;
#if defined(HAVE_GCC_INLINE_ASSEMBLY)

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
    ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64) && !defined(QTHREAD_64_BIT_ALIGN_T))

    register unsigned int incrd = incrd;	/* no initializing */
    asm volatile (
	    "1:\tlwarx  %0,0,%1\n\t"
	    "add    %0,%0,%2\n\t"
	    "stwcx. %0,0,%1\n\t"
	    "bne-   1b\n\t"	/* if it failed, try again */
	    "isync"	/* make sure it wasn't all a dream */
	    :"=&b"   (retval)
	    :"r"     (operand), "r"(incr)
	    :"cc", "memory");

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)

    register unsigned long incrd = incrd;	/* no initializing */
    asm volatile (
	    "1:\tldarx  %0,0,%1\n\t"
	    "add    %0,%0,%2\n\t"
	    "stdcx. %0,0,%1\n\t"
	    "bne-   1b\n\t"	/* if it failed, try again */
	    "isync"	/* make sure it wasn't all a dream */
	    :"=&b"   (retval)
	    :"r"     (operand), "r"(incr)
	    :"cc", "memory");

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32) || \
      ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64) && !defined(QTHREAD_64_BIT_ALIGN_T))

    register uint32_t oldval, newval;
    newval = *operand;
    do {
        oldval = newval;
        newval = oldval + incr;
	/* newval always gets the value of *operand; if it's
	 * the same as oldval, then the swap was successful */
        __asm__ __volatile__ ("cas [%1] , %2, %0"
                              : "+r" (newval)
                              : "r" (operand), "r"(oldval)
                              : "cc", "memory");
    } while (oldval != newval);
    retval = oldval + incr;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)

    register aligned_t oldval, newval;
    newval = *operand;
    do {
        oldval = newval;
        newval = oldval + incr;
	/* newval always gets the value of *operand; if it's
	 * the same as oldval, then the swap was successful */
        __asm__ __volatile__ ("casx [%1] , %2, %0"
                              : "+r" (newval)
                              : "r" (operand), "r"(oldval)
                              : "cc", "memory");
    } while (oldval != newval);
    retval = oldval + incr;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)

# if !defined(QTHREAD_64_BIT_ALIGNED_T)
    int32_t res;

    if (incr == 1) {
	asm volatile ("fetchadd4.rel %0=%1,1":"=r" (res)
		      :"m"     (*operand));

	retval = res+1;
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
	} while (res != old);	       /* if res!=old, the calc is out of date */
	retval = res+incr;
    }
# else
    int64_t res;

    if (incr == 1) {
	asm volatile ("fetchadd8.rel %0=%1,1":"=r" (res)
		      :"m"     (*operand));

	retval = res+1;
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
	} while (res != old);	       /* if res!=old, the calc is out of date */
	retval = res+incr;
    }
# endif

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
      ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) && !defined(QTHREAD_64_BIT_ALIGN_T))

    retval = incr;
    asm volatile ("lock ;  xaddl %0, %1;"
		  :"=r"(retval)
		  :"m"(*operand), "0"(retval)
		  : "memory");
    retval += incr;

#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)

    retval = incr;
    asm volatile ("lock xaddq; %0, %1;"
		  :"=r"(retval)
		  :"m"(*operand), "0"(retval)
		  : "memory");
    retval += incr;

#else

#error "Unimplemented assembly architecture"

#endif

#elif defined(QTHREAD_MUTEX_INCREMENT)

    qthread_t *me = qthread_self();

    qthread_lock(me, (void *)operand);
    *operand += incr;
    retval = *operand;
    qthread_unlock(me, (void *)operand);

#else

#error "Neither atomic or mutex increment enabled"

#endif
    return retval;
}				       /*}}} */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _QTHREAD_H_ */
