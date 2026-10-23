#ifndef QT_CONTEXT_H
#define QT_CONTEXT_H

#if defined(USE_SYSTEM_SWAPCONTEXT)

#include <ucontext.h> /* for ucontext_t */
typedef ucontext_t qt_context_t;

#else

#include <qthread/common.h>
#if QTHREAD_ASSEMBLY_ARCH == QTHREAD_UNSUPPORTED
#error Fast context swap not available on current architecture \
       (or the current architecture was not recognized properly.
#endif

#include "fastcontext/taskimpl.h"
typedef uctxt_t qt_context_t;

#endif

#endif // ifndef QT_CONTEXT_H
/* vim:set expandtab: */
