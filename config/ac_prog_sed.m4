# -*- Autoconf -*-
#
# This file is from Autoconf 2.68, for compatibility with older
# Autoconfs.  Original copyright below.
# Remove this file when we drop support for AC 2.59 and older

# This file is part of Autoconf.			-*- Autoconf -*-
# Macros that test for specific, unclassified, features.
#
# Copyright (C) 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001,
# 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Free Software
# Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# As a special exception, the Free Software Foundation gives unlimited
# permission to copy, distribute and modify the configure scripts that
# are the output of Autoconf.  You need not follow the terms of the GNU
# General Public License when using or distributing such scripts, even
# though portions of the text of Autoconf appear in them.  The GNU
# General Public License (GPL) does govern all other use of the material
# that constitutes the Autoconf program.
#
# Certain portions of the Autoconf source text are designed to be copied
# (in certain cases, depending on the input) into the output of
# Autoconf.  We call these the "data" portions.  The rest of the Autoconf
# source text consists of comments plus executable code that decides which
# of the data portions to output in any given case.  We call these
# comments and executable code the "non-data" portions.  Autoconf never
# copies any of the non-data portions into its output.
#
# This special exception to the GPL applies to versions of Autoconf
# released by the Free Software Foundation.  When you make and
# distribute a modified version of Autoconf, you may extend this special
# exception to the GPL to apply to your modified version as well, *unless*
# your modified version has the potential to copy into its output some
# of the text that was the non-data portion of the version that you started
# with.  (In other words, unless your change moves or copies text from
# the non-data portions to the data portions.)  If your modification has
# such potential, you must delete any notice of this special exception
# to the GPL from your modified version.
#
# Written by David MacKenzie, with help from
# Franc,ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
# Roland McGrath, Noah Friedman, david d zuhn, and many others.

# AC_PROG_SED
# -----------
# Check for a fully functional sed program that truncates
# as few characters as possible.  Prefer GNU sed if found.
AC_DEFUN([AC_PROG_SED],
[AC_CACHE_CHECK([for a sed that does not truncate output], ac_cv_path_SED,
    [dnl ac_script should not contain more than 99 commands (for HP-UX sed),
     dnl but more than about 7000 bytes, to catch a limit in Solaris 8 /usr/ucb/sed.
     ac_script=s/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/
     for ac_i in 1 2 3 4 5 6 7; do
       ac_script="$ac_script$as_nl$ac_script"
     done
     echo "$ac_script" 2>/dev/null | sed 99q >conftest.sed
     AS_UNSET([ac_script])
     _AC_PATH_PROGS_FEATURE_CHECK(SED, [sed gsed],
	[_AC_FEATURE_CHECK_LENGTH([ac_path_SED], [ac_cv_path_SED],
		["$ac_path_SED" -f conftest.sed])])])
 SED="$ac_cv_path_SED"
 AC_SUBST([SED])dnl
 rm -f conftest.sed
])# AC_PROG_SED
