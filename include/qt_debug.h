#ifndef QTHREAD_DEBUG_H
#define QTHREAD_DEBUG_H

#include <stdlib.h> /* for malloc() and friends */
#include <sys/types.h>

#include "qt_alloc.h"

#ifndef EXTERNAL_ALLOCATOR
#define MALLOC(sz) qt_malloc(sz)
#define FREE(ptr, sz) qt_free(ptr)
#endif

#endif /* QTHREAD_DEBUG_H */
/* vim:set expandtab: */
