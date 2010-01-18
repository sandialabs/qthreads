#ifndef QT_ATOMICS_H
#define QT_ATOMICS_H

#include <qthread/common.h>
#include <qthread/qthread.h>

#ifdef QTHREAD_NEEDS_IA64INTRIN
# ifdef HAVE_IA64INTRIN_H
#  include <ia64intrin.h>
# elif defined(HAVE_IA32INTRIN_H)
#  include <ia32intrin.h>
# endif
#endif

#if defined(HAVE_PTHREAD_SPIN_INIT) && ! defined(__tile__)
# define QTHREAD_FASTLOCK_INIT(x) pthread_spin_init(&(x), PTHREAD_PROCESS_PRIVATE)
# define QTHREAD_FASTLOCK_LOCK(x) pthread_spin_lock((x))
# define QTHREAD_FASTLOCK_UNLOCK(x) pthread_spin_unlock((x))
# define QTHREAD_FASTLOCK_DESTROY(x) pthread_spin_destroy(&(x))
# define QTHREAD_FASTLOCK_TYPE pthread_spinlock_t
#else
# define QTHREAD_FASTLOCK_INIT(x) pthread_mutex_init(&(x), NULL)
# define QTHREAD_FASTLOCK_LOCK(x) pthread_mutex_lock((x))
# define QTHREAD_FASTLOCK_UNLOCK(x) pthread_mutex_unlock((x))
# define QTHREAD_FASTLOCK_DESTROY(x) pthread_mutex_destroy(&(x))
# define QTHREAD_FASTLOCK_TYPE pthread_mutex_t
#endif

#ifdef QTHREAD_MUTEX_INCREMENT
# include <pthread.h>
# define QTHREAD_CASLOCK(var)	var; QTHREAD_FASTLOCK_TYPE var##_caslock
# define QTHREAD_CASLOCK_INIT(var,i)   var = i; QTHREAD_FASTLOCK_INIT(var##_caslock)
# define QTHREAD_CASLOCK_DESTROY(var)	QTHREAD_FASTLOCK_DESTROY(var##_caslock)
# define QTHREAD_CASLOCK_READ_UI(var)   qt_cas_read_ui((volatile uintptr_t*)&(var), &(var##_caslock))
# define QT_CAS(var,oldv,newv) qt_cas_((void*volatile*)&(var), (void*)(oldv), (void*)(newv), &(var##_caslock))
static QINLINE void* qt_cas_(void*volatile* const ptr, void* const oldv, void* const newv, QTHREAD_FASTLOCK_TYPE *lock)
{
    void * ret;
    QTHREAD_FASTLOCK_LOCK(lock);
    ret = *ptr;
    if (*ptr == oldv) {
	*ptr = newv;
    }
    QTHREAD_FASTLOCK_UNLOCK(lock);
    return ret;
}
static QINLINE uintptr_t qt_cas_read_ui(volatile uintptr_t * const ptr, QTHREAD_FASTLOCK_TYPE *mutex)
{
    uintptr_t ret;
    QTHREAD_FASTLOCK_LOCK(mutex);
    ret = *ptr;
    QTHREAD_FASTLOCK_UNLOCK(mutex);
    return ret;
}
#else
# define QTHREAD_CASLOCK(var)	(var)
# define QTHREAD_CASLOCK_INIT(var,i) (var) = i
# define QTHREAD_CASLOCK_DESTROY(var)
# define QTHREAD_CASLOCK_READ_UI(var) (var)
# ifdef QTHREAD_ATOMIC_CAS_PTR
#  define qt_cas(P,O,N) (void*)__sync_val_compare_and_swap((P),(O),(N))
# else
static QINLINE void* qt_cas(void*volatile* const ptr, void* const oldv, void* const newv)
{/*{{{*/
# if defined(HAVE_GCC_INLINE_ASSEMBLY)
#  if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32)
    void* result;
    __asm__ __volatile__ ("1:\n\t"
	    "lwarx  %0,0,%3\n\t"
	    "cmpw   %0,%1\n\t"
	    "bne    2f\n\t"
	    "stwcx. %2,0,%3\n\t"
	    "bne-   1b\n"
	    "2:"
	    :"=&b" (result)
	    :"r"(oldv), "r"(newv), "r"(ptr)
	    :"cc", "memory");
    return result;
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
    void *nv = newv;
    __asm__ __volatile__
	("cas [%1], %2, %0"
	 : "=&r" (nv)
	 : "r" (ptr), "r"(oldv)
#if !defined(__SUNPRO_C) && !defined(__SUNPRO_CC)
	 , "0"(nv)
#endif
	 : "cc", "memory");
    return nv;
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    void *nv = newv;
    __asm__ __volatile__
	("casx [%1], %2, %0"
	 : "=&r" (nv)
	 : "r" (ptr), "r"(oldv)
#if !defined(__SUNPRO_C) && !defined(__SUNPRO_CC)
	 , "0"(nv)
#endif
	 : "cc", "memory");
    return nv;
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    void ** retval;
    __asm__ __volatile__ ("mov ar.ccv=%0;;": :"rO" (oldv));
    __asm__ __volatile__ ("cmpxchg4.acq %0=[%1],%2,ar.ccv"
	    :"=r"(retval)
	    :"r"(ptr), "r"(newv)
	    :"memory");
    return retval;
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
    void ** retval;
    /* note that this is GNU/Linux syntax (aka AT&T syntax), not Intel syntax.
     * Thus, this instruction has the form:
     * [lock] cmpxchg reg, reg/mem
     *                src, dest
     *
     * NOTE: this is valid even on 64-bit architectures, because AMD64
     * instantiates cmpxchg for 8-byte registers, and IA32 never has 64-bit
     * pointers
     */
    __asm__ __volatile__ ("lock; cmpxchg %1,(%2)"
	    : "=a"(retval) /* store from EAX */
	    : "r"(newv), "r" (ptr),
	      "a"(oldv) /* load into EAX */
	    :"cc","memory");
    return retval;
#  else
#   error "Don't have a qt_cas implementation for this architecture"
#  endif
# else
#  error "CAS needs inline assembly OR __sync_val_compare_and_swap"
# endif
}/*}}}*/
# endif /* ATOMIC_CAS_PTR */
#endif /* MUTEX_INCREMENT */

#endif
