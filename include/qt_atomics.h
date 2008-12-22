#ifndef QT_ATOMICS_H
#define QT_ATOMICS_H

#ifdef QTHREAD_QTOMIC_BUILTINS
#define qt_cas(P,O,N) __sync_val_comare_and_swap((P),(O),(N))
#else
static QINLINE void* qt_cas(volatile void** ptr, void* oldv, void* newv)
{
# if defined(HAVE_GCC_INLINE_ASSEMBLY)
#  if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32)
#   if !defined(GCC_VERSION) && defined(__cplusplus)
    asm volatile
#   else
    __asm__ __volatile__
#   endif
	("casx [%1], %2, %0"
	 : "=&r" (newv)
	 : "r" (ptr), "r"(oldv), "0"(newv)
	 : "cc", "memory");
    return newv;
#  endif
# else
#  error "Don't have a qt_cas implementation for this architecture"
# endif
}
#endif

#endif
