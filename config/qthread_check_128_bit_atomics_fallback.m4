AC_DEFUN([QT_NEEDS_128_BIT_ATOMIC_FALLBACK],
[
AC_CACHE_CHECK([whether Qthreads may need to use the fallback implementation for 128 bit atomics],
               [qt_cv_128b_atomic_fallback],
[
AC_LANG_PUSH([C++])
AC_TRY_COMPILE(
[
// This file compiles only if clang or similar can safely use 128 bit atomics at runtime even in debug builds.
// For that to be the case it needs to be using a sufficiently recent libatomic from gcc or it needs
// to be using compiler_rt for its atomics.
#if __cplusplus >= 202002L
#include <version>
#else
#include <ciso646>
#endif

// Only have a fallback implementation of 128 bit atomics for these architectures,
// so let the others use the standard implementation and hope for the best.
// On OSX and FreeBSD, compiler-rt is used by default instead of libatomic so we don't need to worry about this.
#if defined(__aarch64__) || defined(__arm__) || !(defined(__clang__) && (defined(__APPLE__) || defined(__FreeBSD__)))
// gcc version info as obtained from libstdc++ since it's surprisingly difficult to figure out which gcc clang is using for its support libraries.
// Note: if no C++ compiler is available, this compilation will fail and the fallback will be used since we couldn't confirm
// that the underlying libatomic is safe to use.
// If we're seeing libstdc++ here, that means there's an underlying gcc install also providing libatomic and that libatomic is actually
// what we're using because libstdc++ depends on it.
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 7
#error Too early of a version to have _GLIBCXX_RELEASE, let alone 128 bit atomics.
#endif
#if !defined(_LIBCPP_VERSION) && !defined(__GLIBCXX__)
#error Too early of a libstdc++ version to even include its version info in <ciso646>.
#endif
#if defined(__GLIBCXX__)
#ifdef __amd64__
#if !(_GLIBCXX_RELEASE == 13 || (_GLIBCXX_RELEASE == 12 && __GLIBCXX__ >= 20230508) || (_GLIBCXX_RELEASE == 11 && __GLIBCXX__ >= 20230529))
#error Only more recent versions of gcc have lock-free 128 bit atomics.
#endif
#elif defined(__aarch64__)
#if !(_GLIBCXX_RELEASE == 13)
#error Only more recent versions of gcc have lock-free 128 bit atomics.
#endif
#endif
#endif

// If we're using libc++ we can't get the underlying gcc version from it here so we can't guarantee that
// 128 bit atomics will actually be lock-free. We only have clang use the fallback implementation for
// 128 bit atomics in debug builds anyway and it's extremely unusual to build qthreads with libc++,
// so this case is not super important.
// It's also technically possible for libc++ and clang to just get their atomics from
// compiler-rt instead, but again we don't have an easy way to check that here.
#if defined(_LIBCPP_VERSION)
#error can't guarantee lock-free 128 bit atomics without gcc version info.
#endif

#endif
],
[return 0;],
[qt_cv_128b_atomic_fallback=no],
[qt_cv_128b_atomic_fallback=yes])
AC_LANG_POP([C++])
])
AS_IF([test x$qt_cv_128b_atomic_fallback = xyes],
       [AC_DEFINE([QTHREADS_NEEDS_128_BIT_ATOMIC_FALLBACK], [1],
       [Whether a fallback is needed because the underlying libatomic may not provide lock-free 128 bit atomics])])
])
