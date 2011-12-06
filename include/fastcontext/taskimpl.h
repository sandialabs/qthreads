/* Portions of this file copyright (c) 2005-2006 Russ Cox, MIT; see COPYING */
#ifndef TASKIMPL_H
#define TASKIMPL_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_TILE)
# ifdef HAVE_STDARG_H
#  include <stdarg.h>
# endif
# include <stddef.h>
# define NEEDTILEMAKECONTEXT
# define NEEDSWAPCONTEXT
#endif

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
# define NEEDX86MAKECONTEXT
# define NEEDSWAPCONTEXT
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
# define NEEDX86MAKECONTEXT
# define NEEDSWAPCONTEXT
# define NEEDX86REGISTERARGS
#endif

#if ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
     (QTHREAD_ASSEMBlY_ARCH == QTHREAD_POWERPC64))
# define NEEDPOWERMAKECONTEXT
# define NEEDSWAPCONTEXT
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN32__)
# if ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
      (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64))
#  include "386-ucontext.h"
# elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_TILE)
#  include "tile-ucontext.h"
# elif ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) || \
        (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64))
#  include "power-ucontext.h"
# else
#  error This platform has no ucontext.h header
# endif
#endif

#if 0 && defined(__sun__)
# include "sparc-ucontext.h"
#endif

#endif // ifndef TASKIMPL_H
/* vim:set expandtab: */
