#ifndef QTHREADS_ATOMIC128_H
#define QTHREADS_ATOMIC128_H

#include <stdatomic.h>
#include <stdint.h>

// 128 Bit atomic operations aren't consistently exposed in the default standard atomic implementation
// due to limitations of some older arm architectures preventing standard atomics from working with
// immutable memory. Because of this, we wrap a third-party implementation here instead of trying to
// use something like _Atomic __int128. We need these to be lock-free because they're used for
// optimistic loads of whole structures and the lock-based fallback implementation for atomics
// doesn't accout for mixed size atomic accesses (e.g. atomically loading 128 bits while writing the
// last 64 bits of that 128 bit block elsewhere).

// TODO: Make the 128 bit atomics optional since using mixed-size atomics is relying on
// architecture-defined behavior and not standard defined behavior.
// That should make trying other architectures easier.

// user can define QTHREADS_USE_STANDARD_128_BIT_ATOMICS in their flags to override this detection logic.
#ifndef QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#ifdef __x86_64__
#ifdef __AVX__
// Intel and AMD both specify that 128 bit loads and stores are atomic (with reasonable alignment constraints)
// on all processors with AVX, so if we know we're running on a processor with AVX and have a recent enough
// gcc to use that fact, we can use the standard atomics and they will be lock-free. Otherwise fall back to
// the less-optimized version vendored in from progress64 that uses the old cmpxchg16b instruction.
//
// Note: VIA and Zhaoxin don't specify whether this is true for their x86 processors
// so gcc doesn't assume these are atomic and implements this via locks which will likely break things for us,
// but it's also impossible for us to check for them using the preprocessor.
// Those processors are currently very rare though, so we're not bothering with them here.
// TODO: add an init-time cpuid check where we can raise more informative errors for unsupported cases like this.
//
// gcc doesn't rely on those atomicity guarantees until more recent bugfix releases, so check for those and
// then fall back to the vendored implementation which just uses the old cmpxchg16b instruction if it would use locks.
// With clang, it'll just inline the appropriate atomic instructions as long as optimizations are on.
// In debug mode it falls back to the not quite equivalent libatomic implementation from gcc though.
// In our configure script we set clang up to tell us which gcc version it's using via the __GNUC__ and __GNUC_MINOR__
// defines instead of its old weird defaults so we can check for the libatomic version at compile time here.
// This all works assuming that qthreads is never compiled with a newer libatomic than is available at runtime.
// icc also just passes through the __GNUC__ values from the underlying gcc it's using.
// icx and acfl just do what clang does.
#if defined(__clang__) && defined(__OPTIMIZE__)
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#elif __GNUC__ >= 13 || (__GNUC__ == 12 && __GNUC_MINOR__ >= 3) || (__GNUC__ == 11 && __GNUC_MINOR__ >= 4)
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#endif
#endif // #ifdef __AVX__

#elif defined(__aarch64__)
#if __ARM_ARCH < 8
#error "Qthreads is not compatible with arm versions earlier than 8."
#endif
#if defined(__clang) && defined(__OPTIMIZE__)
// clang inlines the 128 bit atomic loads on arm as long as optimizations are on, but falls back to
// the gcc libatomic implementation (which isn't always equivalent) when optimizations aren't on.
// At build time we have clang pipe the underlying gcc's version info through __GNUC__ and __GNUC_MINOR__
// so that logic works as a fallback when optimizations are off.
// Again, this all works assuming that qthreads is never compiled with a newer libatomic than is available at runtime.
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#elif __GNUC__ >= 13
#if __ARM_ARCH > 8 && __ARM_ARCH != 801 && __ARM_ARCH != 802 && __ARM_ARCH != 803
// gcc only provides lock-free 128 bit atomics in libatomic for armv8.4 and later.
// We want arm 8.4 or later, but there's inconsistency with how to detect arm versions.
// After 8.1 it's supposed to be a 3 digit number including minor version info, but that's not actually the case yet.
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109415
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#elif __ARM_ARCH == 8 && defined(__ARM_FEATURE_ATOMICS) && defined(__ARM_FEATURE_BTI)
// This is a next-best attempt to detect v8.4 or later. __ARM_FEATURE_BTI is defined on 8.5 and later.
// There's no discernible compile-time difference between 8.3 and 8.4 unless the version macro bug gets fixed
// so the only other option is to specify QTHREADS_USE_STANDARD_128_BIT_ATOMICS manually at
// build time on armv8.4. Without that it'll just fall back to our vendored 128 bit atomics which
// may be slower but will still be lock-free (and correct for the speculative loading idioms currently in qthreads).
// TODO: at init-time, check that we're not running standard atomics with armv8.3 or earlier.
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#endif
#endif
#elif defined(__POWERPC__) || defined(_ARCH_PPC) || defined(_ARCH_PPC64)
// We don't have a fallback implementation for powerpc, but the corresponding instructions are already available
// as of power8 and have been for a very long time so gcc should do the right thing.
#define QTHREADS_USE_STANDARD_128_BIT_ATOMICS
#else
#error "Unsupported Architecture"
#endif
#endif // #ifndef QTHREADS_USE_STANDARD_128_BIT_ATOMICS

typedef unsigned __int128 qt_uint128;
typedef __int128 qt_int128;
typedef struct {uint64_t _a, b;} qt_128b;

#ifdef QTHREADS_USE_STANDARD_128B_ATOMICS

static inline void qt_atomic_store_explicit_128(volatile qt_128b *obj, qt_128b desired, memory_order order) {
  atomic_store_explicit((_Atomic qt_128b*)obj, desired, order);
}

static inline qt_128b qt_atomic_load_explicit_128(const volatile qt_128b *obj, memory_order order) {
  return atomic_load_explicit((_Atomic qt_128b*)obj, order);
}

static inline qt_128b qt_atomic_exchange_explicit_128(volatile qt_128b *obj, qt_128b desired, memory_order order) {
  return atomic_exchange_explicit((_Atomic qt_128b*)obj, desired, order);
}

static inline _Bool qt_atomic_compare_exchange_weak_explicit_128(volatile qt_128b *obj, qt_128b *expected, qt_128b desired, memory_order succ, memory_order fail) {
  return atomic_compare_exchange_weak_explicit((_Atomic qt_128b*)obj, expected, desired, succ, fail);
}

static inline _Bool qt_atomic_compare_exchange_strong_explicit_128(volatile qt_128b *obj, qt_128b *expected, qt_128b desired, memory_order succ, memory_order fail) {
  return atomic_compare_exchange_strong_explicit((_Atomic qt_128b*)obj, expected, desired, succ, fail);
}

#else //QTHREADS_USE_STANDARD_128B_ATOMICS

#include "progress64/lockfree.h"

// Bitwise cast without triggering any kind of implicit conversion.
// VAL must be an lvalue, not an rvalue.
#define QT_BITCAST(T, VAL) (*(T*)&VAL)

static inline void qt_atomic_store_explicit_128(volatile qt_128b *obj, qt_128b desired, memory_order order) {
  __int128 d = QT_BITCAST(__int128, desired);
  lockfree_store_16((__int128*)obj, d, order);
}

static inline qt_128b qt_atomic_load_explicit_128(const volatile qt_128b *obj, memory_order order) {
  __int128 ret = lockfree_load_16((__int128*)obj, order);
  return QT_BITCAST(qt_128b, ret);
}

static inline qt_128b qt_atomic_exchange_explicit_128(volatile qt_128b *obj, qt_128b desired, memory_order order) {
  __int128 d = QT_BITCAST(__int128, desired);
  __int128 ret = lockfree_exchange_16((__int128*)obj, d, order);
  return QT_BITCAST(qt_128b, ret);
}

static inline _Bool qt_atomic_compare_exchange_weak_explicit_128(volatile qt_128b *obj, qt_128b *expected, qt_128b desired, memory_order succ, memory_order fail) {
  __int128 d = QT_BITCAST(__int128, desired);
  return lockfree_compare_exchange_16((__int128*)obj, (__int128*)expected, d, true, succ, fail);
}

static inline _Bool qt_atomic_compare_exchange_strong_explicit_128(volatile qt_128b *obj, qt_128b *expected, qt_128b desired, memory_order succ, memory_order fail) {
  __int128 d = QT_BITCAST(__int128, desired);
  return lockfree_compare_exchange_16((__int128*)obj, (__int128*)expected, d, false, succ, fail);
}

#endif //QTHREADS_USE_STANDARD_128B_ATOMICS

#endif
