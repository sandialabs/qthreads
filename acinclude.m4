dnl -*- autoconf -*-
dnl
dnl Copyright (c) 2008 Sandia Corporation

m4_include([config/ax_create_stdint_h.m4])
m4_include([config/ac_compile_check_sizeof.m4])
# Only include this if we're using an old Autoconf.  Remove when we
# finally drop support for AC 2.59
m4_if(m4_version_compare(m4_defn([m4_PACKAGE_VERSION]), [2.60]), -1,
      [m4_include([config/ac_prog_cc_c99.m4])])
