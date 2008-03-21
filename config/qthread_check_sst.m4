# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_SST([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_SST], [
SST_INCLUDE=
SST_LIBS=

AC_ARG_WITH([sst],
  [AS_HELP_STRING([--with-sst[[=PATH_TO_SST]]],
    [compiles the SST version of qthreads, relying on the SST includes being in the specified directory. If 'yes', then the includes are assumed to be specified in the CPPFLAGS.])],
  [], [with_sst=no])

AC_ARG_WITH([sst-std-libs],
  [AS_HELP_STRING([--with-sst-std-libs=DIR],
    [DIR contains the standard run-time libraries necessary for linking SST applications])])
AC_ARG_WITH([statlibs],
  [AS_HELP_STRING([--with-statlibs=DIR],
    [Same as --with-sst-std-libs, for backward compatibility])],
  [AS_IF([test -z "$with_sst_std_libs"], [with_sst_std_libs="$with_statlibs"])])

if test "$with_sst" != "no" ; then
  CPPFLAG_SAVE="$CPPFLAGS"
  if test "with_sst" != "yes" ; then
    SST_INCLUDE="-I$with_sst/Struct_Simulator/serialProto/ -I$with_sst/Struct_Simulator/serialProto/ssFrontEnd/ -I$with_sst/Struct_Simulator/pimSrc/ppc/"
  fi

  CPPFLAGS="$CPPFLAGS $SST_INCLUDE"
  AC_CHECK_HEADERS([ppcPimCalls.h pimSysCallDefs.h pimSysCallTypes.h], [],
    [AC_MSG_ERROR(["Is your SST setup complete and specified with --with-sst=<path_to_SST>?"])])
  CPPFLAGS="$CPPFLAGS"
  if test ! -z "$with_sst_std_libs" ; then
    SST_LIBS="-L$with_sst_std_libs"
  fi
  
  dnl this avoids the problems with __TEXT,pic* sections
  SST_CFLAGS="-fno-pic"
  dnl this avoids the stupid fprintf$LDBLStub warnings
  SST_INCLUDE="$SST_INCLUDE -D__LDBL_MANT_DIG__=53"

  AC_DEFINE([SST], [1], [Use SST to implement qthreads API])
fi

AC_SUBST(SST_INCLUDE)
AC_SUBST(SST_LIBS)
AC_SUBST(SST_CFLAGS)

AS_IF([test "$with_sst" != "no"], [$1], [$2])
])
