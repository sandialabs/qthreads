AC_DEFUN([QTHREAD_CHECK_SYSCALLTYPES],[
AC_CHECK_DECLS([SYS_nanosleep,SYS_sleep,SYS_usleep,SYS_system,SYS_select,SYS_wait4,SYS_pread],
    [],[],[[#include <sys/syscall.h>]])
AC_CHECK_SIZEOF([socklen_t],[],[[#include <sys/socket.h>]])
AS_IF([test "$ac_cv_sizeof_socklen_t" -eq 4],
	  [socklentype=uint32_t],
	  [AS_IF([test "$ac_cv_sizeof_socklen_t" -eq 8],
	         [socklentype=uint64_t],
			 [AC_MSG_ERROR([socklen_t is an unfortunate size])])])
AC_DEFINE_UNQUOTED([QT_SOCKLENTYPE_T],[$socklentype],[socklen_t compatible uint])
AM_CONDITIONAL([HAVE_DECL_SYS_SYSTEM], [test "x$ac_cv_have_decl_sys_system" == xyes])
AM_CONDITIONAL([HAVE_DECL_SYS_SELECT], [test "x$ac_cv_have_decl_sys_select" == xyes])
AM_CONDITIONAL([HAVE_DECL_SYS_WAIT4], [test "x$ac_cv_have_decl_sys_wait4" == xyes])
AM_CONDITIONAL([HAVE_DECL_SYS_PREAD], [test "x$ac_cv_have_decl_sys_pread" == xyes])
])
