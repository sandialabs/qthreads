# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_ALIGNED_ATTRIBUTE([action-if-found], [action-if-not-found])
# -------------------------------------------------------------------------
AC_DEFUN([QTHREAD_ALIGNED_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for aligned data declarations],
 [qt_cv_aligned_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
int foo __attribute__((aligned(64)));
int f(int i) { foo = 1; return foo; }]])],
 [qt_cv_aligned_attr=yes],
 [qt_cv_aligned_attr=no])])
 AS_IF([test "x$qt_cv_aligned_attr" = xyes],
 	   [AC_DEFINE([QTHREAD_ALIGNEDDATA_ALLOWED], [1],
		   [specifying data alignment is allowed])])
 AS_IF([test "x$qt_cv_aligned_attr" = xyes], [$1], [$2])
])

AC_DEFUN([QTHREAD_MALLOC_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((malloc))],
 [qt_cv_malloc_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((malloc))
void * f(int i) { return malloc(i); }]])],
 [qt_cv_malloc_attr=yes],
 [qt_cv_malloc_attr=no])])
 AS_IF([test "x$qt_cv_malloc_attr" = xyes],
 	   [defstr="__attribute__((malloc))"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([Q_MALLOC], [$defstr],
		   [if the compiler supports __attribute__((malloc))])
 AS_IF([test "x$qt_cv_malloc_attr" = xyes], [$1], [$2])
])

AC_DEFUN([QTHREAD_UNUSED_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((unused))],
 [qt_cv_unused_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((unused))
int f(int i) { return i; }]])],
 [qt_cv_unused_attr=yes],
 [qt_cv_unused_attr=no])])
 AS_IF([test "x$qt_cv_unused_attr" = xyes],
 	   [unusedstr="__attribute__((unused))"],
	   [unusedstr=""])
 AC_DEFINE_UNQUOTED([Q_UNUSED], [$unusedstr],
		   [most gcc compilers know a function __attribute__((unused))])
 AS_IF([test "x$qt_cv_unused_attr" = xyes], [$1], [$2])
])

AC_DEFUN([QTHREAD_NOINLINE_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((noinline))],
 [qt_cv_noinline_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((noinline))
void * f(int i) { return malloc(i); }]])],
 [qt_cv_noinline_attr=yes],
 [qt_cv_noinline_attr=no])])
 AS_IF([test "x$qt_cv_noinline_attr" = xyes],
 	   [defstr="__attribute__((noinline))"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([Q_NOINLINE], [$defstr],
		   [if the compiler supports __attribute__((NOINLINE))])
 AS_IF([test "x$qt_cv_noinline_attr" = xyes], [$1], [$2])
])
