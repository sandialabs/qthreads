# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_ATOMICS([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_ATOMICS], [
AC_CHECK_HEADERS([ia64intrin.h ia32intrin.h])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS],
  [qthread_cv_atomic_CAS],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
long bar=1, old=1, new=2;
long foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[qthread_cv_atomic_CAS="yes"],
		[qthread_cv_atomic_CAS="no"])])
AC_CACHE_CHECK([whether compiler supports builtin atomic incr],
  [qthread_cv_atomic_incr],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
long bar=1;
long foo = __sync_fetch_and_add(&bar, 1);
return foo;
}]])],
		[qthread_cv_atomic_incr="yes"],
		[qthread_cv_atomic_incr="no"])
   ])
AS_IF([test "$qthread_cv_atomic_CAS" = "yes"],
	  [AC_CACHE_CHECK([whether ia64intrin.h is required],
	    [qthread_cv_require_ia64intrin_h],
		[AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int main()
{
long bar=1, old=1, new=2;
long foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[qthread_cv_require_ia64intrin_h="no"],
		[qthread_cv_require_ia64intrin_h="yes"])])])
AS_IF([test "$qthread_cv_require_ia64intrin_h" = "yes"],
	  [AC_DEFINE([QTHREAD_NEEDS_IA64INTRIN],[1],[if this header is necessary for builtin atomics])])
AS_IF([test "$qthread_cv_atomic_CAS" = "yes"],
	[AC_DEFINE([QTHREAD_ATOMIC_CAS],[1],[if the compiler supports __sync_val_compare_and_swap])])
AS_IF([test "$qthread_cv_atomic_incr" = "yes"],
	[AC_DEFINE([QTHREAD_ATOMIC_INCR],[1],[if the compiler supports __sync_fetch_and_add])])
AS_IF([test "$qthread_cv_atomic_CAS" = "yes" -a "$qthread_cv_atomic_incr" = "yes"],
  		[AC_DEFINE([QTHREAD_ATOMIC_BUILTINS],[1],[if the compiler supports __sync_val_compare_and_swap])
		 $1],
		[$2])
])
