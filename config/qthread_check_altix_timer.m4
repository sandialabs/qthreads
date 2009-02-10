# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_ALTIX_TIMER([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_ALTIX_TIMER], [
AC_CHECK_HEADERS([sn/mmtimer.h linux/mmtimer.h],
  [timer_altix_happy="yes"
  break],
  [timer_altix_happy="no"])

AS_IF([test "$timer_altix_happy" = "yes"],
      [AC_CHECK_HEADERS([sys/ioctl.h sys/mman.h])])

AS_IF([test "$timer_altix_happy" = "yes"],
      [AC_CACHE_CHECK([if MM timer can be opened],
         [qthread_cv_mm_timer_mmap],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#if HAVE_SN_MMTIMER_H
# include <sn/mmtimer.h>
#elif HAVE_LINUX_MMTIMER_H
# include <linux/mmtimer.h>
#endif
#ifndef MMTIMER_FULLNAME
# define MMTIMER_FULLNAME "/dev/mmtimer"
#endif
], [
    int fd;
    fd = open(MMTIMER_FULLNAME, O_RDONLY);
    if (-1 ==fd) return 1;])],
            [qthread_cv_mm_timer_mmap="yes"],
            [qthread_cv_mm_timer_mmap="no"])])
AS_IF([test "$qthread_cv_mm_timer_mmap" = "yes"],
      [timer_altix_happy="yes"],
      [timer_altix_happy="no"])])

AS_IF([test "$timer_altix_happy" = "yes"], [$1], [$2])
])
