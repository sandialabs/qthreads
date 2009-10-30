#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <qthread/cacheline.h>

static int cacheline_bytes = 0;

static void figure_out_cacheline_size()
{
#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    if (sizeof(long) == 4) {
	cacheline_bytes = 32; // G4
    } else {
	cacheline_bytes == 128; // G5
    }
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    cacheline_bytes = 128;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    cacheline_bytes = 128; // Itanium L2/L3 are 128, L1 is 64
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
# if !defined(HAVE_GCC_INLINE_ASSEMBLY)
    cacheline_bytes = 128;
# else
    int op = 1, eax, ebx, ecx, edx;
    int tmp = 0;
#  if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
#   ifdef __PIC__
    __asm__ __volatile__ ("push %%ebx\n\t"
			  "cpuid\n\t"
			  "mov %%ebx, %1\n\t"
			  "pop %%ebx"
			  :"=a"(eax), "=m"(ebx), "=c"(ecx), "=d"(edx)
			  :"a"(op));
#   else
    __asm__ __volatile__("cpuid"
	    :"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	    :"a"(op));
#   endif
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
    __asm__ __volatile__("cpuid"
	    :"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	    :"a"(op));
#  endif
    tmp = 8*((ebx>>8)&0xff);
    if (tmp == 0) {
	op = 2;
#  if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
	__asm__ __volatile__ ("push %%ebx\n\t"
		"cpuid\n\t"
		"mov %%ebx, %1\n\t"
		"pop %ebx"
		:"=a"(eax), "=m"(ebx), "=c"(ecx), "=d"(edx)
		:"a"(op));
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
	__asm__ __volatile__ ("cpuid"
		:"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
		:"a"(op));
#  endif
	tmp = 8*((ebx>>8)&0xff); /* XXX THIS IS PROBABLY WRONG */
    }
    cacheline_bytes = tmp;
# endif
#else
    cacheline_bytes = 128; // safe default, probably not accurate
#endif
}

/* returns the cache line size */
int qt_cacheline()
{
    if (cacheline_bytes == 0)
	figure_out_cacheline_size();
    return cacheline_bytes;
}
