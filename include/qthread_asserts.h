#ifndef QTHREAD_ASSERTS_H
#define QTHREAD_ASSERTS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef QTHREAD_NO_ASSERTS
# include <assert.h> /* for assert() */
#endif

#ifdef QTHREAD_NO_ASSERTS
# define qassert(op, val) op
# define qassertnot(op, val) op
# ifdef assert
#  undef assert
# endif
# define assert(foo)
#else
# define qassert(op, val) assert(op == val)
# define qassertnot(op, val) assert(op != val)
#endif

#endif
