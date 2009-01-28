#ifndef QT_ATOMICS_H
#define QT_ATOMICS_H

#include <qthread/common.h>

#if (HAVE_IA64INTRIN_H && QTHREAD_NEEDS_IA64INTRIN)
# include <ia64intrin.h>
#endif

#ifdef QTHREAD_ATOMIC_CAS
#define qt_cas(P,O,N) (void*)__sync_val_compare_and_swap((P),(O),(N))
#else
static QINLINE void* qt_cas(volatile void** ptr, void* oldv, void* newv)
{
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
#   if defined(__SUNPRO_CC)
    asm volatile
#   else
    __asm__ __volatile__
#   endif
	("cas [%1], %2, %0"
	 : "=&r" (newv)
	 : "r" (ptr), "r"(oldv), "0"(newv)
	 : "cc", "memory");
    return newv;
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
#   if defined(__SUNPRO_CC)
    asm volatile
#   else
    __asm__ __volatile__
#   endif
	("casx [%1], %2, %0"
	 : "=&r" (newv)
	 : "r" (ptr), "r"(oldv), "0"(newv)
	 : "cc", "memory");
    return newv;
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
     */
    __asm__ __volatile__ ("lock; cmpxchg %1,%2"
	    : "=&a"(retval) /* store from EAX */
	    : "r"(newv), "m" (*ptr),
	      "0"(oldv) /* load into EAX */
	    :"cc","memory");
    return retval;
#  else
#   error "Don't have a qt_cas implementation for this architecture"
#  endif
# else
#  error "CAS needs inline assembly OR __sync_val_compare_and_swap"
# endif
}
#endif

#endif
