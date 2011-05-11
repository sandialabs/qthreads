AC_DEFUN([QTHREAD_CHECK_SYSCALLTYPES],[
AC_CHECK_SIZEOF([socklen_t],[],[[#include <sys/socket.h>]])
AS_IF([test "$ac_cv_sizeof_socklen_t" -eq 4],
	  [socklentype=uint32_t],
	  [AS_IF([test "$ac_cv_sizeof_socklen_t" -eq 8],
	         [socklentype=uint64_t],
			 [AC_MSG_ERROR([socklen_t is an unfortunate size])])])
AC_DEFINE_UNQUOTED([QT_SOCKLENTYPE_T],[$socklentype],[socklen_t compatible uint])
AM_CONDITIONAL([HAVE_DECL_SYS_SYSTEM], [test "x$ac_cv_have_decl_sys_system" == xyes])
])
