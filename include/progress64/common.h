//Copyright (c) 2018, ARM Limited. All rights reserved.
//
//SPDX-License-Identifier:        BSD-3-Clause

#ifndef _COMMON_H
#define _COMMON_H

//Compiler hints
#define ALWAYS_INLINE __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#ifdef __clang__
#define UNROLL_LOOPS __attribute__((opencl_unroll_hint(8)))
#else
#define UNROLL_LOOPS __attribute__((optimize("unroll-loops")))
#endif
#define INIT_FUNCTION __attribute__((constructor))
#define LIKELY(x)    __builtin_expect(!!(x), 1)
#define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define COMPILER_MEMORY_FENCE() __asm __volatile("" ::: "memory")
#define UNREACHABLE() __builtin_unreachable()

#ifdef NDEBUG
#if defined __GNUC__ && __GNUC__ >= 8
#define ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#else
#define ASSUME(cond) (void)(cond)
#endif
#else //Assertions enabled, check that assumptions are true
#define ASSUME(cond) assert(cond)
#endif

//Hardware hints
#define PREFETCH_FOR_READ(ptr) __builtin_prefetch((ptr), 0, 3)
#define PREFETCH_FOR_WRITE(ptr) __builtin_prefetch((ptr), 1, 3)

//Use GNUC syntax for ALIGNED
#define ALIGNED(x) __attribute__((__aligned__(x)))
#if __STDC_VERSION__ >= 201112L
//Use C11 syntax
#define THREAD_LOCAL _Thread_local
#else
//Use GNUC syntax
#define THREAD_LOCAL __thread
#endif

#define ROUNDUP_POW2(x) \
    ({ \
         unsigned long _x = (x); \
         _x > 1 ?  (1UL << (__SIZEOF_LONG__ * __CHAR_BIT__ - __builtin_clzl(_x - 1UL))) : 1; \
     })

/*
 * By Hallvard B Furuseth
 * https://groups.google.com/forum/?hl=en#!msg/comp.lang.c/attFnqwhvGk/sGBKXvIkY3AJ
 * Return (v ? floor(log2(v)) : 0) when 0 <= v < 1<<[8, 16, 32, 64].
 * Inefficient algorithm, intended for compile-time constants.
 */
#define LOG2_8BIT(v)  (8 - 90/(((v)/4+14)|1) - 2/((v)/2+1))
#define LOG2_16BIT(v) (8*((v)>255) + LOG2_8BIT((v) >>8*((v)>255)))
#define LOG2_32BIT(v) \
    (16*((v)>65535L) + LOG2_16BIT((v)*1L >>16*((v)>65535L)))
#define LOG2_64BIT(v)\
    (32*((v)/2L>>31 > 0) \
     + LOG2_32BIT((v)*1L >>16*((v)/2L>>31 > 0) \
			 >>16*((v)/2L>>31 > 0)))

#define ROUNDUP(a, b) \
    ({                                          \
        __typeof__ (a) tmp_a = (a);             \
        __typeof__ (b) tmp_b = (b);             \
        ((tmp_a + tmp_b - 1) / tmp_b) * tmp_b;  \
    })

#define MIN(a, b) \
    ({                                          \
        __typeof__ (a) tmp_a = (a);             \
        __typeof__ (b) tmp_b = (b);             \
        tmp_a < tmp_b ? tmp_a : tmp_b;          \
    })

#define MAX(a, b) \
    ({                                          \
        __typeof__ (a) tmp_a = (a);             \
        __typeof__ (b) tmp_b = (b);             \
        tmp_a > tmp_b ? tmp_a : tmp_b;          \
    })

#define IS_POWER_OF_TWO(n) \
    ({                                            \
	__typeof__ (n) tmp_n = (n);               \
	tmp_n != 0 && (tmp_n & (tmp_n - 1)) == 0; \
    })

#define SWAP(_a, _b) \
{ \
    __typeof__ (_a) _t; \
    _t = _a; \
    _a = _b; \
    _b = _t; \
}

#if __SIZEOF_POINTER__ == 4
typedef unsigned long long ptrpair_t;//assume 64 bits
#else //__SIZEOF_POINTER__ == 8
typedef __int128 ptrpair_t;
#endif

#endif
