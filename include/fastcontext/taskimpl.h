/* Portions of this file are Copyright (c) 2025 Tactical Computing Labs, LLC;
 * see COPYING */

#ifndef TASKIMPL_H
#define TASKIMPL_H

#include "qthread/common.h"

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32)
#include "386-ucontext.h"
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
#define NEEDX86REGISTERARGS
#include "386-ucontext.h"
#elif ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32) ||                         \
       (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64))
#include "power-ucontext.h"
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_ARM)
#include "arm-ucontext.h"
#include <stdarg.h>
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_RISCV)
#include "riscv-ucontext.h"
#include <stdarg.h>
#elif (QTHREAD_ASSEMBLY_ARCH == QTHREAD_ARMV8_A64)
#include "arm-ucontext.h"
#include <stdarg.h>
#else
#error This platform has no fastcontext support
#endif

#endif // ifndef TASKIMPL_H
/* vim:set expandtab: */
