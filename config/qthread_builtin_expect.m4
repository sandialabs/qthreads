AC_DEFUN([QTHREAD_BUILTIN_EXPECT],
[AC_CACHE_CHECK([for __builtin_expect],
  [qthread_cv_builtin_expect],
  [SAVE_CFLAGS="$CFLAGS"
   CFLAGS="-Werror $CFLAGS"
   AC_LINK_IFELSE([AC_LANG_SOURCE([[
int main()
{
	int i;
	if (__builtin_expect(i==0, 0)) { return 0; }
	return 1;
}]])],
	[qthread_cv_builtin_expect="yes"],
	[qthread_cv_builtin_expect="no"])
   CFLAGS="$SAVE_CFLAGS"])
 AS_IF([test "x$qthread_cv_builtin_expect" = "xyes"],
 	   [expectfunc="__builtin_expect(!!(x),(y))"],
 	   [expectfunc="(x)"])
 AC_DEFINE_UNQUOTED([QTHREAD_EXPECT(x,y)],
 					[$expectfunc],
					[The expect function, if it exists])
])
