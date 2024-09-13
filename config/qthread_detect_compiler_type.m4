# -*- Autoconf -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AC_DEFUN([_QTHREAD_CHECK_IFDEF],
 [AC_PREPROC_IFELSE(
    [AC_LANG_PROGRAM([[]],[[#ifndef $1
#error $1 not defined
#endif]])],
    [$2],[$3])])

AC_DEFUN([_QTHREAD_CHECK_IFDEF_EQ],
 [AC_PREPROC_IFELSE(
    [AC_LANG_PROGRAM([[]],[[#ifndef $1
#error $1 not defined
#elif (($1) != ($2))
#error $1 does not equal $2
#endif]])],
    [$3],[$4])])

# QTHREAD_DETECT_COMPILER_TYPE
# These #defs are based on the list at http://predef.sourceforge.net/precomp.html
# Moved at some point to http://sourceforge.net/p/predef/wiki/Compilers
# ------------------------------------------------------------------
AC_DEFUN([QTHREAD_DETECT_COMPILER_TYPE], [
AC_CACHE_CHECK([what kind of C compiler $CC is],
  [qthread_cv_c_compiler_type],
  [AC_LANG_PUSH([C])

   dnl These compilers have been caught pretending to be GNU GCC
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__INTEL_COMPILER],[qthread_cv_c_compiler_type=Intel])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__clang__],[qthread_cv_c_compiler_type=Clang])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__llvm__],[
	    qthread_cv_c_compiler_type=LLVM
		_QTHREAD_CHECK_IFDEF_EQ([__APPLE_CC__],[5658],[qthread_cv_c_compiler_type=Apple-LLVM-5658])
		AS_IF([test "x$qthread_cv_c_compiler_type" = "xLLVM"],
		      [_QTHREAD_CHECK_IFDEF([__APPLE_CC__],[qthread_cv_c_compiler_type=Apple-LLVM])])
		])])

   dnl GCC is one of the most common
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__GNUC__],[
	    qthread_cv_c_compiler_type=GNU
		_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[2],[qthread_cv_c_compiler_type=GNU2])
		AS_IF([test "x$qthread_cv_c_compiler_type" = "xGNU"],
			  [_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[3],[qthread_cv_c_compiler_type=GNU3])])
		AS_IF([test "x$qthread_cv_c_compiler_type" = "xGNU"],
			  [_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[4],[qthread_cv_c_compiler_type=GNU4])])
		AS_IF([test "x$qthread_cv_c_compiler_type" != "xGNU"],
		      [_QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[0],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.0"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[1],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.1"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[2],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.2"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[3],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.3"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[4],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.4"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[5],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.5"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[6],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.6"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[7],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.7"])
		       _QTHREAD_CHECK_IFDEF_EQ([__GNUC_MINOR__],[8],[qthread_cv_c_compiler_type="${qthread_cv_c_compiler_type}.8"])
			   _QTHREAD_CHECK_IFDEF([__APPLE_CC__],[qthread_cv_c_compiler_type="Apple-${qthread_cv_c_compiler_type}"])
			  ])
	 ])])

   dnl A few common compilers (to detect quickly)
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__CYGWIN__],[qthread_cv_c_compiler_type=Cygwin])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MINGW32__],[qthread_cv_c_compiler_type=MinGW32])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MINGW64__],[qthread_cv_c_compiler_type=MinGW64])])

   dnl Now detect the rarer ones
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MRC__],[qthread_cv_c_compiler_type=MPW])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_MSC_VER],[qthread_cv_c_compiler_type=MicrosoftVisual])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_MRI],[qthread_cv_c_compiler_type=Microtec])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__NDPC__],[qthread_cv_c_compiler_type=MicrowayNDP])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([MIRACLE],[qthread_cv_cxx_compiler_type=Miracle])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__CC_NORCROFT],[qthread_cv_c_compiler_type=Norcroft])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__NWCC__],[qthread_cv_c_compiler_type=NWCC])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__PACIFIC__],[qthread_cv_c_compiler_type=Pacific])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_PACC_VER],[qthread_cv_c_compiler_type=Palm])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__POCC__],[qthread_cv_c_compiler_type=Pelles])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__RENESAS__],[qthread_cv_c_compiler_type=Renesas])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__SASC],[qthread_cv_c_compiler_type=SAS])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_SCO_DS],[qthread_cv_c_compiler_type=SCO])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([SDCC],[qthread_cv_c_compiler_type=SmallDevice])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__SNC__],[qthread_cv_c_compiler_type=SN])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__VOSC__],[qthread_cv_c_compiler_type=StratusVOS])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TenDRA__],[qthread_cv_c_compiler_type=TenDRA])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TI_COMPILER_VERSION__],[qthread_cv_c_compiler_type=TexasInstruments])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([THINKC3],[qthread_cv_c_compiler_type=THINK])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([THINKC4],[qthread_cv_c_compiler_type=THINK])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TINYC__],[qthread_cv_c_compiler_type=TinyC])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TURBOC__],[qthread_cv_c_compiler_type=Turbo])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_UCC],[qthread_cv_c_compiler_type=Ultimate])])
   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__USLC__],[qthread_cv_c_compiler_type=USL])])

   AS_IF([test "x$qthread_cv_c_compiler_type" == x],
     [qthread_cv_c_compiler_type=unknown])
   AC_LANG_POP([C])
  ])
AC_CACHE_CHECK([what kind of C++ compiler $CXX is],
  [qthread_cv_cxx_compiler_type],
  [AC_LANG_PUSH([C++])

   dnl These compilers have been caught pretending to be GNU G++
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__INTEL_COMPILER],[qthread_cv_cxx_compiler_type=Intel])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__clang__],[qthread_cv_cxx_compiler_type=Clang])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__llvm__],[qthread_cv_cxx_compiler_type=LLVM])])

   dnl GCC is one of the most common
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__GNUC__],[
	    qthread_cv_cxx_compiler_type=GNU
		AS_IF([test "x$qthread_cv_cxx_compiler_type" = "xGNU"],
			  [_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[2],[qthread_cv_cxx_compiler_type=GNU2])])
		AS_IF([test "x$qthread_cv_cxx_compiler_type" = "xGNU"],
			  [_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[3],[qthread_cv_cxx_compiler_type=GNU3])])
		AS_IF([test "x$qthread_cv_cxx_compiler_type" = "xGNU"],
			  [_QTHREAD_CHECK_IFDEF_EQ([__GNUC__],[4],[qthread_cv_cxx_compiler_type=GNU4])])
		])])

   dnl A few common compilers (to detect quickly)
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__CYGWIN__],[qthread_cv_cxx_compiler_type=Cygwin])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MINGW32__],[qthread_cv_cxx_compiler_type=MinGW32])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MINGW64__],[qthread_cv_cxx_compiler_type=MinGW64])])

   dnl Now detect the rarer ones
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__MRC__],[qthread_cv_cxx_compiler_type=MPW])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_MSC_VER],[qthread_cv_cxx_compiler_type=MicrosoftVisual])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_MRI],[qthread_cv_cxx_compiler_type=Microtec])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_PACC_VER],[qthread_cv_cxx_compiler_type=Palm])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__RENESAS__],[qthread_cv_cxx_compiler_type=Renesas])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_SCO_DS],[qthread_cv_cxx_compiler_type=SCO])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__SC__],[qthread_cv_cxx_compiler_type=Symantec])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TenDRA__],[qthread_cv_cxx_compiler_type=TenDRA])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TI_COMPILER_VERSION__],[qthread_cv_cxx_compiler_type=TexasInstruments])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__TURBOC__],[qthread_cv_cxx_compiler_type=Turbo])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([_UCC],[qthread_cv_cxx_compiler_type=Ultimate])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__WATCOMC__],[qthread_cv_cxx_compiler_type=Watcom])])
   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__ZTC__],[qthread_cv_cxx_compiler_type=Zortech])])

   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [_QTHREAD_CHECK_IFDEF([__EDG__],[qthread_cv_cxx_compiler_type=EDG_FrontEnd])])

   AS_IF([test "x$qthread_cv_cxx_compiler_type" == x],
     [qthread_cv_cxx_compiler_type=unknown])
   AC_LANG_POP([C++])
  ])
])
