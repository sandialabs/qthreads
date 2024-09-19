# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

AC_DEFUN([QTHREAD_BUILTIN_SYNCHRONIZE],[dnl
AC_REQUIRE([QTHREAD_CHECK_ASSEMBLY])
AC_CACHE_CHECK([support for __sync_synchronize],
 [qt_cv_builtin_synchronize],
 [AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],[[__sync_synchronize();]])],
 [qt_cv_builtin_synchronize=yes],
 [qt_cv_builtin_synchronize=no])])
 AS_IF([test "x$qt_cv_asm_volatile" = "xyes"],
       [cdefstr='__asm__ __volatile__ ("":::"memory")'],
	   [cdefstr='do { } while(0)'])
 AS_IF([test "x$qt_cv_builtin_synchronize" == xyes],
	   [mdefstr='__sync_synchronize()'],
       [AS_IF([test "x$qt_cv_gcc_inline_assembly" = "xyes"],
	          [case "$qthread_cv_asm_arch" in
		     AMD64)
			   case "$host" in
			     mic-*)
				   mdefstr='__asm__ __volatile__ ("lock; addl %0,0(%%esp)" ::"i"(0): "memory")'
				   ;;
				 *)
                       mdefstr='__asm__ __volatile__ ("mfence":::"memory")'
				   ;;
			   esac
		       ;;
		     POWERPC*)
                       mdefstr='__asm__ __volatile__ ("sync":::"memory")'
		       ;;
		    *)
				 AC_MSG_ERROR([ASM $qthread_cv_asm_arch])
                       mdefstr="$cdefstr"
		       ;;
		       esac])])
 AC_DEFINE_UNQUOTED([MACHINE_FENCE], [$mdefstr],
   [if the compiler supports __sync_synchronize (fallback to COMPILER_FENCE)])
 AC_DEFINE_UNQUOTED([COMPILER_FENCE], [$cdefstr],
   [if the compiler supports inline assembly, we can prevent reordering])
 AS_IF([test "x$qt_cv_builtin_synchronize" == xyes], [$1], [$2])
])
