//Copyright (c) 2017, ARM Limited. All rights reserved.
//
//SPDX-License-Identifier:        BSD-3-Clause

#ifndef _LOCKFREE_AARCH64_H
#define _LOCKFREE_AARCH64_H

#include <stdbool.h>
#include "common.h"

#include "ldxstx.h"

#ifdef __ARM_FEATURE_ATOMICS
ALWAYS_INLINE
static inline __int128 casp(__int128 *var, __int128 old, __int128 neu, int mo)
{
#if __GNUC__ >= 9
    //GCC-9 is supposed to allocate even/odd register pairs
    if (mo == __ATOMIC_RELAXED)
    {
	__asm __volatile("casp %0, %H0, %1, %H1, [%2]"
			: "+r" (old)
			: "r" (neu), "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_ACQUIRE)
    {
	__asm __volatile("caspa %0, %H0, %1, %H1, [%2]"
			: "+r" (old)
			: "r" (neu), "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_ACQ_REL)
    {
	__asm __volatile("caspal %0, %H0, %1, %H1, [%2]"
			: "+r" (old)
			: "r" (neu), "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_RELEASE)
    {
	__asm __volatile("caspl %0, %H0, %1, %H1, [%2]"
			: "+r" (old)
			: "r" (neu), "r" (var)
			: "memory");
    }
    else
    {
	abort();
    }
    return old;
#else
    register uint64_t x0 __asm ("x0") = (uint64_t)old;
    register uint64_t x1 __asm ("x1") = (uint64_t)(old >> 64);
    register uint64_t x2 __asm ("x2") = (uint64_t)neu;
    register uint64_t x3 __asm ("x3") = (uint64_t)(neu >> 64);
    if (mo == __ATOMIC_RELAXED)
    {
	__asm __volatile("casp %[old1], %[old2], %[neu1], %[neu2], [%[v]]"

			: [old1] "+r" (x0), [old2] "+r" (x1)
			: [neu1] "r" (x2), [neu2] "r" (x3), [v] "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_ACQUIRE)
    {
	__asm __volatile("caspa %[old1], %[old2], %[neu1], %[neu2], [%[v]]"
			: [old1] "+r" (x0), [old2] "+r" (x1)
			: [neu1] "r" (x2), [neu2] "r" (x3), [v] "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_ACQ_REL)
    {
	__asm __volatile("caspal x0, %[old2], %[neu1], %[neu2], [%[v]]"
			: [old1] "+r" (x0), [old2] "+r" (x1)
			: [neu1] "r" (x2), [neu2] "r" (x3), [v] "r" (var)
			: "memory");
    }
    else if (mo == __ATOMIC_RELEASE)
    {
	__asm __volatile("caspl %[old1], %[old2], %[neu1], %[neu2], [%[v]]"
			: [old1] "+r" (x0), [old2] "+r" (x1)
			: [neu1] "r" (x2), [neu2] "r" (x3), [v] "r" (var)
			: "memory");
    }
    else
    {
	abort();
    }
    return x0 | ((__int128)x1 << 64);
#endif
}
#endif

ALWAYS_INLINE
static inline bool lockfree_compare_exchange_16(register __int128 *var, __int128 *exp, register __int128 neu, bool weak, int mo_success, int mo_failure)
{
#ifdef __ARM_FEATURE_ATOMICS
    (void)weak; (void)mo_failure;
    __int128 old, expected = *exp;
    old = casp(var, expected, neu, mo_success);
    *exp = old;//Always update, atomically read value
    return old == expected;
#else
    (void)weak;//Always do strong CAS or we can't perform atomic read
    (void)mo_failure;//Ignore memory ordering for failure, memory order for
    //success must be stronger or equal
    int ldx_mo = MO_LOAD(mo_success);
    int stx_mo = MO_STORE(mo_success);
    register __int128 old, expected = *exp;
    __asm __volatile("" ::: "memory");
    do
    {
	//Atomicity of LDX16 is not guaranteed
	old = ldx128(var, ldx_mo);
	//Must write back neu or old to verify atomicity of LDX16
    }
    while (UNLIKELY(stx128(var, old == expected ? neu : old, stx_mo)));
    *exp = old;//Always update, atomically read value
    return old == expected;
#endif
}

ALWAYS_INLINE
static inline bool lockfree_compare_exchange_16_frail(register __int128 *var, __int128 *exp, register __int128 neu, bool weak, int mo_success, int mo_failure)
{
#ifdef __ARM_FEATURE_ATOMICS
    (void)weak; (void)mo_failure;
    __int128 old, expected = *exp;
    old = casp(var, expected, neu, mo_success);
    *exp = old;//Always update, atomically read value
    return old == expected;
#else
    (void)weak;//Weak CAS and non-atomic load on failure
    (void)mo_failure;//Ignore memory ordering for failure, memory order for
    //success must be stronger or equal
    int ldx_mo = MO_LOAD(mo_success);
    int stx_mo = MO_STORE(mo_success);
    register __int128 expected = *exp;
    __asm __volatile("" ::: "memory");
    //Atomicity of LDX16 is not guaranteed
    register __int128 old = ldx128(var, ldx_mo);
    if (LIKELY(old == expected && !stx128(var, neu, stx_mo)))
    {
	//Right value and STX succeeded
	__asm __volatile("" ::: "memory");
	return 1;
    }
    __asm __volatile("" ::: "memory");
    //Wrong value or STX failed
    *exp = old;//Old possibly torn value (OK for 'frail' flavour)
    return 0;//Failure, *exp updated
#endif
}

ALWAYS_INLINE
static inline __int128 lockfree_load_16(__int128 *var, int mo)
{
    __int128 old = *var;//Possibly torn read
    //Do CAS to ensure atomicity
    //Either CAS succeeds (writing back the same value)
    //Or CAS fails and returns the old value (atomic read)
    (void)lockfree_compare_exchange_16(var, &old, old, /*weak=*/false, mo, mo);
    return old;
}

ALWAYS_INLINE
static inline void lockfree_store_16(__int128 *var, __int128 neu, int mo)
{
#ifdef __ARM_FEATURE_ATOMICS
    __int128 old, expected;
    do
    {
	expected = *var;
	old = casp(var, expected, neu, mo);
    }
    while (old != expected);
#else
    int ldx_mo = __ATOMIC_ACQUIRE;
    int stx_mo = MO_STORE(mo);
    do
    {
	(void)ldx128(var, ldx_mo);
    }
    while (UNLIKELY(stx128(var, neu, stx_mo)));
#endif
}

ALWAYS_INLINE
static inline __int128 lockfree_exchange_16(__int128 *var, __int128 neu, int mo)
{
#ifdef __ARM_FEATURE_ATOMICS
    __int128 old, expected;
    do
    {
	expected = *var;
	old = casp(var, expected, neu, mo);
    }
    while (old != expected);
    return old;
#else
    int ldx_mo = MO_LOAD(mo);
    int stx_mo = MO_STORE(mo);
    register __int128 old;
    do
    {
	old = ldx128(var, ldx_mo);
    }
    while (UNLIKELY(stx128(var, neu, stx_mo)));
    return old;
#endif
}

ALWAYS_INLINE
static inline __int128 lockfree_fetch_and_16(__int128 *var, __int128 mask, int mo)
{
#ifdef __ARM_FEATURE_ATOMICS
    __int128 old, expected;
    do
    {
	expected = *var;
	old = casp(var, expected, expected & mask, mo);
    }
    while (old != expected);
    return old;
#else
    int ldx_mo = MO_LOAD(mo);
    int stx_mo = MO_STORE(mo);
    register __int128 old;
    do
    {
	old = ldx128(var, ldx_mo);
    }
    while (UNLIKELY(stx128(var, old & mask, stx_mo)));
    return old;
#endif
}

ALWAYS_INLINE
static inline __int128 lockfree_fetch_or_16(__int128 *var, __int128 mask, int mo)
{
#ifdef __ARM_FEATURE_ATOMICS
    __int128 old, expected;
    do
    {
	expected = *var;
	old = casp(var, expected, expected | mask, mo);
    }
    while (old != expected);
    return old;
#else
    int ldx_mo = MO_LOAD(mo);
    int stx_mo = MO_STORE(mo);
    register __int128 old;
    do
    {
	old = ldx128(var, ldx_mo);
    }
    while (UNLIKELY(stx128(var, old | mask, stx_mo)));
    return old;
#endif
}

#define _ATOMIC_UMAX_4_DEFINED
ALWAYS_INLINE
static inline uint32_t
lockfree_fetch_umax_4(uint32_t *var, uint32_t val, int mo)
{
    uint32_t old;
#ifdef __ARM_FEATURE_ATOMICS
    if (mo == __ATOMIC_RELAXED)
    {
	__asm __volatile("ldumax %w1, %w0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_ACQUIRE)
    {
	__asm __volatile("ldumaxa %w1, %w0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_RELEASE)
    {
	__asm __volatile("ldumaxl %w1, %w0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_ACQ_REL)
    {
	__asm __volatile("ldumaxl %w1, %w0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else
    {
	abort();
    }
#else
    do
    {
	old = ldx32(var, MO_LOAD(mo));
	if (val <= old)
	{
	    return old;
	}
	//Else val > old, update
    }
    while (UNLIKELY(stx32(var, val, MO_STORE(mo))));
#endif
    return old;
}

#define _ATOMIC_UMAX_8_DEFINED
ALWAYS_INLINE
static inline uint64_t
lockfree_fetch_umax_8(uint64_t *var, uint64_t val, int mo)
{
    uint64_t old;
#ifdef __ARM_FEATURE_ATOMICS
    if (mo == __ATOMIC_RELAXED)
    {
	__asm __volatile("ldumax %x1, %x0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_ACQUIRE)
    {
	__asm __volatile("ldumaxa %x1, %x0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_RELEASE)
    {
	__asm __volatile("ldumaxl %x1, %x0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else if (mo == __ATOMIC_ACQ_REL)
    {
	__asm __volatile("ldumaxal %x1, %x0, [%x2]"
			:"=&r"(old)
			:"r"(val), "r"(var)
			:"memory");
    }
    else
    {
	abort();
    }
#else
    do
    {
	old = ldx64(var, MO_LOAD(mo));
	if (val <= old)
	{
	    return old;
	}
	//Else val > old, update
    }
    while (UNLIKELY(stx64(var, val, MO_STORE(mo))));
#endif
    return old;
}

#endif
