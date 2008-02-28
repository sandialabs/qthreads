# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# QTHREAD_CHECK_ATTRIBUTE_ALIGNED([action-if-found], [action-if-not-found])
# -------------------------------------------------------------------------
AC_DEFUN([QTHREAD_CHECK_ATTRIBUTE_ALIGNED], [
happy=yes
case "$host" in
  *-solaris*)
    happy=no
  ;;
esac
AS_IF([test "$GCC" = yes -o "$happy" = yes], [$1], [$2])
])
