//Copyright (c) 2018, ARM Limited. All rights reserved.
//
//SPDX-License-Identifier:        BSD-3-Clause

#ifndef _LOCKFREE__X86_64_H
#define _LOCKFREE__X86_64_H

#include <stdbool.h>
#include "common.h"

union u128
{
    struct
    {
	uint64_t lo, hi;
    } s;
    __int128 ui;
};

ALWAYS_INLINE
static inline bool cmpxchg16b(__int128 *src, union u128 *cmp, union u128 with)
{
    bool result;
    __asm__ __volatile__
	("lock cmpxchg16b %1\n\tsetz %0"
	 : "=q" (result), "+m" (*src), "+d" (cmp->s.hi), "+a" (cmp->s.lo)
	 : "c" (with.s.hi), "b" (with.s.lo)
	 : "cc", "memory"
	);
    return result;
}

ALWAYS_INLINE
static inline bool lockfree_compare_exchange_16(register __int128 *var, __int128 *exp, __int128 neu, bool weak, int mo_success, int mo_failure)
{
    (void)weak;
    (void)mo_success;
    (void)mo_failure;
    union u128 cmp, with;
    cmp.ui = *exp;
    with.ui = neu;
    bool ret = cmpxchg16b(var, &cmp, with);
    if (UNLIKELY(!ret))
    {
	*exp = cmp.ui;
    }
    return ret;
}

ALWAYS_INLINE
static inline __int128 lockfree_load_16(__int128 *var, int mo)
{
    (void)mo;
    __int128 old = *var;
    do
    {
    }
    while (!lockfree_compare_exchange_16(var, &old, old, false, 0, 0));
    return old;
}

ALWAYS_INLINE
static inline void lockfree_store_16(__int128 *var, __int128 neu, int mo)
{
    (void)mo;
    __int128 old = *var;
    do
    {
    }
    while (!lockfree_compare_exchange_16(var, &old, neu, false, 0, 0));
}

ALWAYS_INLINE
static inline __int128 lockfree_exchange_16(__int128 *var, __int128 neu, int mo)
{
    (void)mo;
    __int128 old = *var;
    do
    {
    }
    while (!lockfree_compare_exchange_16(var, &old, neu, false, 0, 0));
    return old;
}

ALWAYS_INLINE
static inline __int128 lockfree_fetch_and_16(__int128 *var, __int128 mask, int mo)
{
    (void)mo;
    __int128 old = *var;
    do
    {
    }
    while (!lockfree_compare_exchange_16(var, &old, old & mask, false, 0, 0));
    return old;
}

ALWAYS_INLINE
static inline __int128 lockfree_fetch_or_16(__int128 *var, __int128 mask, int mo)
{
    (void)mo;
    __int128 old = *var;
    do
    {
    }
    while (!lockfree_compare_exchange_16(var, &old, old | mask, false, 0, 0));
    return old;
}

#endif
