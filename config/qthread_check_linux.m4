# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_LINUX([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_LINUX], [
AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>

int main() {
  return sysconf(_SC_NPROCESSORS_CONF);
}]])],
  [AC_DEFINE([HAVE_SC_NPROCESSORS_CONF], [1], [define if you have _SC_NPROCESSORS_CONF])])

AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>

int main() {
  int name[2] = { CTL_HW, HW_NCPU };
  unsigned int oldv;
  unsigned int oldvlen = sizeof(oldv);
  return sysctl(name, &oldv, &oldvlen, NULL, 0);
}]])],
  [AC_DEFINE([HAVE_HW_NCPU], [1], [define if you have HW_NCPU and CTL_HW])])
])
