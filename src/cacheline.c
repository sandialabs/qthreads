#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <qthread/cacheline.h>
#include <qthread/common.h>
#include <stdio.h>
//#define DEBUG_CPUID 1

enum vendor { AMD, Intel, Unknown };
static int cacheline_bytes = 0;

#define MAX(a,b) (((a)>(b))?(a):(b))

static void cpuid(int op, int *eax_ptr, int *ebx_ptr, int *ecx_ptr,
		  int *edx_ptr)
{				       /*{{{ */
    int eax, ebx, ecx, edx;

#  if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
#   ifdef __PIC__
    __asm__ __volatile__("push %%ebx\n\t" "cpuid\n\t" "mov %%ebx, %1\n\t"
			 "pop %%ebx":"=a"(eax), "=m"(ebx), "=c"(ecx),
			 "=d"(edx)
			 :"a"    (op));
#   else
    __asm__ __volatile__("cpuid":"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			 :"a"    (op));
#   endif
#  elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
    __asm__ __volatile__("cpuid":"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			 :"a"    (op));
#  endif
    *eax_ptr = eax;
    *ebx_ptr = ebx;
    *ecx_ptr = ecx;
    *edx_ptr = edx;
}				       /*}}} */

static void descriptor(int d)
{				       /*{{{ */
    switch (d) {
	case 0x00:
	    return;
	case 0x0a:
	case 0x0c:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
#ifdef DEBUG_CPUID
	    printf("\top 2: code %02x: 32\n", d);
#endif
	    cacheline_bytes = MAX(cacheline_bytes, 32);
	    return;
	case 0x0d:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x25:
	case 0x29:
	case 0x2c:
	case 0x39:
	case 0x3a:
	case 0x3b:
	case 0x3c:
	case 0x3d:
	case 0x3e:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x60:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x78:
	case 0x79:
	case 0x7a:
	case 0x7b:
	case 0x7c:
	case 0x7d:
	case 0x7f:
	case 0x86:
	case 0x87:
	case 0xd0:
	case 0xd1:
	case 0xd2:
	case 0xd6:
	case 0xd7:
	case 0xd8:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xe2:
	case 0xe3:
	case 0xe4:
	case 0xea:
	case 0xeb:
	case 0xec:
#ifdef DEBUG_CPUID
	    printf("\top 2: code %02x: 64\n", d);
#endif
	    cacheline_bytes = MAX(cacheline_bytes, 64);
	    return;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x06:
	case 0x08:
	case 0x09:
	case 0x30:
	case 0x40:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x55:
	case 0x56:
	case 0x57:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0xb0:
	case 0xb1:
	case 0xb2:
	case 0xb3:
	case 0xb4:
	case 0xca:
	case 0xf0:
	case 0xf1:
	    return;
	default:		       /*printf("no idea: %02x\n", d); */
	    return;
    }
}				       /*}}} */

static void examine(int r, char *str)
{				       /*{{{ */
    if ((r & 0x40000000) == 0) {
	descriptor((r >> 0) & 0xff);
	descriptor((r >> 8) & 0xff);
	descriptor((r >> 16) & 0xff);
	descriptor((r >> 24) & 0xff);
    }
}				       /*}}} */

static void figure_out_cacheline_size()
{				       /*{{{ */
#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64)
    if (sizeof(long) == 4) {
	cacheline_bytes = 32;	       // G4
    } else {
	cacheline_bytes == 128;	       // G5
    }
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64)
    cacheline_bytes = 128;
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64)
    cacheline_bytes = 128;	       // Itanium L2/L3 are 128, L1 is 64
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
# if !defined(HAVE_GCC_INLINE_ASSEMBLY)
    cacheline_bytes = 128;
# else
    int eax, ebx, ecx, edx;
    enum vendor v;
    int tmp = 0;
    int largest_ext = 0;
    int largest_std = 0;

    cpuid(0, &eax, &ebx, &ecx, &edx);
    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e) {
	largest_std = eax;
	v = Intel;
#ifdef DEBUG_CPUID
	printf("GenuineIntel (%i max)\n", largest_std);
#endif
    } else if (ebx == 0x68747541 && ecx == 0x444d4163 && edx == 0x69746e65) {
	largest_std = eax;
	v = AMD;
#ifdef DEBUG_CPUID
	printf("AuthenticAMD (%i max)\n", largest_std);
#endif
    } else {
	v = Unknown;
#ifdef DEBUG_CPUID
	printf("Unknown Vendor: %x %x %x %x\n", eax, ebx, ecx, edx);
#endif
    }

    if (v == AMD && largest_std >= 1) {
	cpuid(1, &eax, &ebx, &ecx, &edx);
	tmp = 8 * ((ebx >> 8) & 0xff);     // The clflush width
#ifdef DEBUG_CPUID
	printf("clflush width: %i\n", tmp);
#endif
	cacheline_bytes = MAX(cacheline_bytes, tmp);
    }
    if (v == Intel) {
	if (largest_std >= 2) {
	    int i = 1;
	    int limit;

	    do {
		cpuid(2, &eax, &ebx, &ecx, &edx);
		limit = eax & 0xf;
		i++;
		examine(eax, "eax");
		examine(ebx, "ebx");
		examine(ecx, "ecx");
		examine(edx, "edx");
	    } while (i < limit);
	}

	if (largest_std >= 4) {
	    // Deterministic cache parameters
	    cpuid(4, &eax, &ebx, &ecx, &edx);
	    tmp = (ebx & 0xfff) + 1;
#ifdef DEBUG_CPUID
	    printf("System Coherency Line Size: %i\n", tmp);
#endif
	    cacheline_bytes = tmp;
	    return;
	}
    }
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
#ifdef DEBUG_CPUID
    printf("largest ext = %x\n", eax);
#endif
    largest_ext = eax;
    if (v == AMD && largest_ext >= 0x80000005) {
	cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
	tmp = (ecx >> 8) & 0xff;
#ifdef DEBUG_CPUID
	printf("L1 cache line size: %i\n", tmp);
#endif
	cacheline_bytes = MAX(cacheline_bytes, tmp);
    }
    if ((v == AMD || v == Intel) && largest_ext >= 0x80000006) {
	cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
	tmp = ecx & 0xff;
#ifdef DEBUG_CPUID
	printf("L2 cache line size: %i\n", tmp);
#endif
	cacheline_bytes = MAX(cacheline_bytes, tmp);
	if (v == AMD) {
	    tmp = edx & 0xff;
#ifdef DEBUG_CPUID
	    printf("L3 cache line size: %i\n", tmp);
#endif
	    cacheline_bytes = MAX(cacheline_bytes, tmp);
	}
    }
# endif
#else
    cacheline_bytes = 128;	       // safe default, probably not accurate
#endif
}				       /*}}} */

/* returns the cache line size */
int qthread_cacheline()
{				       /*{{{ */
    if (cacheline_bytes == 0) {
	figure_out_cacheline_size();
	if (cacheline_bytes == 0) {    /* to cache errors in cacheline detection */
	    cacheline_bytes = 128;
	}
    }
    return cacheline_bytes;
}				       /*}}} */
