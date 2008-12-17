# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_ATOMICS([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_ATOMICS], [
AC_CACHE_CHECK([whether compiler supports builtin atomics],
  [qthread_cv_atomic_builtins],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int main()
{
long bar=1, old=1, new=2;
long foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
        [qthread_cv_atomic_builtins="yes"],
        [qthread_cv_atomic_builtins="no"])])
  AS_IF([test "$qthread_cv_atomic_builtins" = "yes"],
  		[AC_DEFINE([QTHREAD_ATOMIC_BUILTINS],[1],[if the compiler supports __sync_val_compare_and_swap])
		 $1],
		[$2])
])
