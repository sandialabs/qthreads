dnl -*- Autoconf -*-
dnl
dnl Copyright (c)      2008  Sandia Corporation
dnl
dnl This macro based heavily on the ompi_check_asm macro found in 
dnl Open MPI.  License for the original macro is below

dnl Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
dnl                         University Research and Technology
dnl                         Corporation.  All rights reserved.
dnl Copyright (c) 2004-2007 The University of Tennessee and The University
dnl                         of Tennessee Research Foundation.  All rights
dnl                         reserved.
dnl Copyright (c) 2004-2006 High Performance Computing Center Stuttgart, 
dnl                         University of Stuttgart.  All rights reserved.
dnl Copyright (c) 2004-2006 The Regents of the University of California.
dnl                         All rights reserved.
dnl Copyright (c) 2006-2007 Los Alamos National Security, LLC.  All rights
dnl                         reserved. 
dnl Copyright (c) 2006-2007 Cisco Systems, Inc.  All rights reserved.
dnl Copyright (c) 2006-2007 Voltaire, Inc. All rights reserved.
dnl Copyright (c) 2006      Sandia National Laboratories. All rights reserved.
dnl Copyright (c) 2006-2007 Sun Microsystems, Inc.  All rights reserved.
dnl                         Use is subject to license terms.
dnl Copyright (c) 2006-2007 The University of Houston. All rights reserved.
dnl Copyright (c) 2006      Myricom, Inc.  All rights reserved.
dnl Copyright (c) 2007-2008 UT-Battelle, LLC. All rights reserved.
dnl
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl - Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl
dnl - Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer listed
dnl   in this license in the documentation and/or other materials


dnl QTHREAD_TRY_ASSEMBLE(asm-code, [action-if-success], [action-if-fail])
dnl
dnl Attempt to assemble asm-code.  If success, run action-if-success.
dnl Otherwise, run action-if-fail.  Neither action-if-success nor
dnl action-if-fail are required.
dnl
dnl No preprocessing is guaranteed to be done on asm-code.  Some
dnl compilers do not run the preprocessor on assembly files.  
dnl
dnl On failure, asm-test.s will be included in config.out
AC_DEFUN([QTHREAD_TRY_ASSEMBLE],
[cat >conftest.s <<EOF
[$1]
EOF
if test "$CC" = "$CCAS" ; then
    qthread_assemble="$CCAS $CCASFLAGS -c conftest.s >conftest.out 2>&1"
else
    qthread_assemble="$CCAS $CCASFLAGS -o conftest.o conftest.s >conftest.out 2>&1"
fi
if AC_TRY_EVAL(qthread_assemble); then
  # save the warnings
  cat conftest.out >&AC_FD_CC
  ifelse([$2],,:,[$2])
else
  # save compiler output and failed program
  cat conftest.out >&AC_FD_CC
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.s >&AC_FD_CC
  ifelse([$3],,:,[$3])
fi
rm -rf conftest*
unset qthread_assemble
])dnl

AC_DEFUN([QTHREAD_CHECK_ASSEMBLY],[
  AC_REQUIRE([AM_PROG_AS])
  AC_CHECK_SIZEOF([long])

  # find our architecture for purposes of assembly stuff
  qthread_cv_asm_arch="UNSUPPORTED"
  qthread_gcc_inline_assign=""
  case "${host}" in
    i?86-*|x86_64*|mic-*)
      AS_IF([test "$ac_cv_sizeof_long" = "4"],
            [qthread_cv_asm_arch="IA32"],
			[qthread_cv_asm_arch="AMD64"])
      qthread_gcc_inline_assign='"movl [$]0, %0" : "=&r"(ret)'
    ;;

	armv7l-*)
	  qthread_cv_asm_arch="ARM"
	  qthread_gcc_inline_assign='"movt %0, #5" : "=&r"(ret)'
	;;

	*aarch64*)
		qthread_cv_asm_arch="ARMV8_A64"
	;;

    powerpc*|powerpc64*)
      AS_IF([test "$ac_cv_sizeof_long" = "4"],
            [qthread_cv_asm_arch="POWERPC32"],
            [qthread_cv_asm_arch="POWERPC64"])
      qthread_ppc_abi="QTHREAD_PPC_ABI_UNKNOWN"
      _QTHREAD_CHECK_IFDEF([_CALL_AIX],[qthread_ppc_abi=QTHREAD_PPC_ABI_AIX])
      AS_IF([test "$qthread_ppc_abi" = QTHREAD_PPC_ABI_UNKNOWN],
	    [_QTHREAD_CHECK_IFDEF([_CALL_DARWIN],[qthread_ppc_abi=QTHREAD_PPC_ABI_DARWIN])])
      AS_IF([test "$qthread_ppc_abi" = QTHREAD_PPC_ABI_UNKNOWN],
            [_QTHREAD_CHECK_IFDEF([_CALL_SYSV],[qthread_ppc_abi=QTHREAD_PPC_ABI_SYSV])])
      AS_IF([test "$qthread_ppc_abi" = QTHREAD_PPC_ABI_UNKNOWN],
	    [_QTHREAD_CHECK_IFDEF([__APPLE__],[qthread_ppc_abi=QTHREAD_PPC_ABI_DARWIN])])
      AS_IF([test "$qthread_ppc_abi" = QTHREAD_PPC_ABI_UNKNOWN],
	    [_QTHREAD_CHECK_IFDEF([__linux__],
	      [_QTHREAD_CHECK_IFDEF([__PPC__],[qthread_ppc_abi=QTHREAD_PPC_ABI_SYSV])
	       _QTHREAD_CHECK_IFDEF([__PPC64__],[qthread_ppc_abi=QTHREAD_PPC_ABI_AIX])])])
      AC_DEFINE([QTHREAD_PPC_ABI_UNKNOWN], [0], [Constant for an unknown PPC ABI])
      AC_DEFINE([QTHREAD_PPC_ABI_DARWIN], [1], [Constant for the Darwin PPC ABI])
      AC_DEFINE([QTHREAD_PPC_ABI_SYSV], [2], [Constant for the SysV PPC ABI])
      AC_DEFINE([QTHREAD_PPC_ABI_AIX], [3], [Constant for the AIX PPC ABI])
      AC_DEFINE_UNQUOTED([QTHREAD_PPC_ABI],[$qthread_ppc_abi],[Define to specify the PPC ABI])
      qthread_gcc_inline_assign='"A_%=: li %0,0" : "=&r"(ret)'
    ;;

  esac

  AC_MSG_CHECKING([for asssembly architecture])
  AC_MSG_RESULT([$qthread_cv_asm_arch])
  result="QTHREAD_$qthread_cv_asm_arch"
  AC_DEFINE_UNQUOTED([QTHREAD_ASSEMBLY_ARCH], [$result],
      [Architecture type of assembly to use])

  unset result asm_format
])dnl
