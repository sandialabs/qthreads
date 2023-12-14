//Copyright (c) 2016, ARM Limited. All rights reserved.
//
//SPDX-License-Identifier:        BSD-3-Clause

#ifndef _LDXSTX_H
#define _LDXSTX_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __aarch64__

/******************************************************************************
 * ARMv8/A64 load/store exclusive primitives
 *****************************************************************************/

static inline uint32_t ldx8(const uint8_t *var, int mm)
{
    uint32_t old;
    if (mm == __ATOMIC_ACQUIRE)
    __asm volatile("ldaxrb %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("ldxrb %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else
	abort();
    return old;
}

//Return 0 on success, 1 on failure
static inline uint32_t stx8(uint8_t *var, uint32_t neu, int mm)
{
    uint32_t ret;
    if (mm == __ATOMIC_RELEASE)
    __asm volatile("stlxrb %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("stxrb %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else
	abort();
    return ret;
}

static inline uint32_t ldx16(const uint16_t *var, int mm)
{
    uint32_t old;
    if (mm == __ATOMIC_ACQUIRE)
    __asm volatile("ldaxrh %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("ldxrh %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else
	abort();
    return old;
}

//Return 0 on success, 1 on failure
static inline uint32_t stx16(uint16_t *var, uint32_t neu, int mm)
{
    uint32_t ret;
    if (mm == __ATOMIC_RELEASE)
    __asm volatile("stlxrh %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("stxrh %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else
	abort();
    return ret;
}

static inline uint32_t ldx32(const uint32_t *var, int mm)
{
    uint32_t old;
    if (mm == __ATOMIC_ACQUIRE)
    __asm volatile("ldaxr %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("ldxr %w0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else
	abort();
    return old;
}

//Return 0 on success, 1 on failure
static inline uint32_t stx32(uint32_t *var, uint32_t neu, int mm)
{
    uint32_t ret;
    if (mm == __ATOMIC_RELEASE)
    __asm volatile("stlxr %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("stxr %w0, %w1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else
	abort();
    return ret;
}

static inline uint64_t ldx64(const uint64_t *var, int mm)
{
    uint64_t old;
    if (mm == __ATOMIC_ACQUIRE)
    __asm volatile("ldaxr %0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("ldxr %0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else
	abort();
    return old;
}

//Return 0 on success, 1 on failure
static inline uint32_t stx64(uint64_t *var, uint64_t neu, int mm)
{
    uint32_t ret;
    if (mm == __ATOMIC_RELEASE)
    __asm volatile("stlxr %w0, %1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("stxr %w0, %1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else
	abort();
    return ret;
}

static inline __int128 ldx128(const __int128 *var, int mm)
{
    __int128 old;
    if (mm == __ATOMIC_ACQUIRE)
    __asm volatile("ldaxp %0, %H0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("ldxp %0, %H0, [%1]"
                   : "=&r" (old)
                   : "r" (var)
                   : "memory");
    else
	abort();
    return old;
}

//Return 0 on success, 1 on failure
static inline uint32_t stx128(__int128 *var, __int128 neu, int mm)
{
    uint32_t ret;
    if (mm == __ATOMIC_RELEASE)
    __asm volatile("stlxp %w0, %1, %H1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else if (mm == __ATOMIC_RELAXED)
    __asm volatile("stxp %w0, %1, %H1, [%2]"
                   : "=&r" (ret)
                   : "r" (neu), "r" (var)
                   : "memory");
    else
	abort();
    return ret;
}

#define ldx(var, mm) \
_Generic((var), \
    uint8_t *: ldx8, \
    uint16_t *: ldx16, \
    uint32_t *: ldx32, \
    const uint32_t *: ldx32, \
    uint64_t *: ldx64, \
    __int128 *: ldx128 \
    )((var), (mm))

#define stx(var, val, mm) \
_Generic((var), \
    uint8_t *: stx8, \
    uint16_t *: stx16, \
    uint32_t *: stx32, \
    uint64_t *: stx64, \
    __int128 *: stx128 \
)((var), (val), (mm))

static inline void *ldxptr(void *var, int mm)
{
    return (void *)ldx((uintptr_t *)var, mm);
}
static inline uint32_t stxptr(void *var, void *val, int mm)
{
    return stx((uintptr_t *)var, (uintptr_t)val, mm);
}

#else
#error Unsupported architecture
#endif

#endif
