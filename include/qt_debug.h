#ifndef QTHREAD_DEBUG_H
#define QTHREAD_DEBUG_H

#include <stdlib.h> /* for malloc() and friends */
#include <sys/types.h>

#include "qt_alloc.h"

#ifndef EXTERNAL_ALLOCATOR
#define ALLOC_SCRIBBLE(ptr, sz)                                                \
  do {                                                                         \
  } while (0)
#define FREE_SCRIBBLE(ptr, sz)                                                 \
  do {                                                                         \
  } while (0)
#define MALLOC(sz) qt_malloc(sz)
#define FREE(ptr, sz) qt_free(ptr)
#else
#define ALLOC_SCRIBBLE(ptr, sz)                                                \
  do {                                                                         \
  } while (0)
#define FREE_SCRIBBLE(ptr, sz)                                                 \
  do {                                                                         \
  } while (0)
#endif

#endif /* QTHREAD_DEBUG_H */
/* vim:set expandtab: */
