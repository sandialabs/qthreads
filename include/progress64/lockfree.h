//Copyright (c) 2018, ARM Limited. All rights reserved.
//
//SPDX-License-Identifier:        BSD-3-Clause

#ifndef _LOCKFREE_H
#define _LOCKFREE_H

#include "common.h"

#define HAS_ACQ(mo) ((mo) != __ATOMIC_RELAXED && (mo) != __ATOMIC_RELEASE)
#define HAS_RLS(mo) ((mo) == __ATOMIC_RELEASE || (mo) == __ATOMIC_ACQ_REL || (mo) == __ATOMIC_SEQ_CST)

#define MO_LOAD(mo) (HAS_ACQ((mo)) ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED)
#define MO_STORE(mo) (HAS_RLS((mo)) ? __ATOMIC_RELEASE : __ATOMIC_RELAXED)

#if defined __aarch64__

#include "lockfree/aarch64.h"
#define lockfree_compare_exchange_pp_frail lockfree_compare_exchange_16_frail
#define lockfree_compare_exchange_pp lockfree_compare_exchange_16

#elif defined __arm__

#define lockfree_compare_exchange_pp_frail __atomic_compare_exchange_8
#define lockfree_compare_exchange_pp __atomic_compare_exchange_8

#elif defined __x86_64__

#include "lockfree/x86-64.h"
#define lockfree_compare_exchange_pp_frail lockfree_compare_exchange_16
#define lockfree_compare_exchange_pp lockfree_compare_exchange_16

#else

#error Unsupported architecture

#endif

#if (__ATOMIC_RELAXED | __ATOMIC_ACQUIRE) != __ATOMIC_ACQUIRE
#error __ATOMIC bit-wise OR hack failed (see XXX)
#endif
#if (__ATOMIC_RELEASE | __ATOMIC_ACQUIRE) != __ATOMIC_RELEASE
#error __ATOMIC bit-wise OR hack failed (see XXX)
#endif

#ifndef _ATOMIC_UMAX_4_DEFINED
#define _ATOMIC_UMAX_4_DEFINED

ALWAYS_INLINE
static inline uint32_t
lockfree_fetch_umax_4(uint32_t *var, uint32_t val, int mo)
{
    uint32_t old = __atomic_load_n(var, __ATOMIC_RELAXED);
    do
    {
	if (val <= old)
	{
	    return old;
	}
    }
    while (!__atomic_compare_exchange_n(var,
					&old,
					val,
					/*weak=*/true,
					MO_LOAD(mo) | MO_STORE(mo),//XXX
					MO_LOAD(mo)));
    return old;
}
#endif

#ifndef _ATOMIC_UMAX_8_DEFINED
#define _ATOMIC_UMAX_8_DEFINED
ALWAYS_INLINE
static inline uint64_t
lockfree_fetch_umax_8(uint64_t *var, uint64_t val, int mo)
{
    uint64_t old = __atomic_load_n(var, __ATOMIC_RELAXED);
    do
    {
	if (val <= old)
	{
	    return old;
	}
    }
    while (!__atomic_compare_exchange_n(var,
					&old,
					val,
					/*weak=*/true,
					MO_LOAD(mo) | MO_STORE(mo),//XXX
					MO_LOAD(mo)));
    return old;
}
#endif

#endif
