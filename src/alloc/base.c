#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>

/* System Headers */
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
#include <malloc.h> /* for memalign() */
#endif
#ifdef HAVE_GETPAGESIZE
#include <unistd.h>
#else
static QINLINE int getpagesize() { return 4096; }
#endif

/* Internal Headers */
#include "qt_alloc.h"
#include "qt_asserts.h"
#include "qt_debug.h"

/* local constants */
size_t _pagesize = 0;

void *qt_malloc(size_t size) { return malloc(size); }

void qt_free(void *ptr) { free(ptr); }

void *qt_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }

void *qt_realloc(void *ptr, size_t size) { return realloc(ptr, size); }

void qt_internal_alignment_init(void) { _pagesize = getpagesize(); }

void *qt_internal_aligned_alloc(size_t alloc_size, uint_fast16_t alignment) {
  return aligned_alloc((size_t) alignment, alloc_size);
}

void qt_internal_aligned_free(void *ptr, uint_fast16_t alignment) {
  qt_free(ptr);
}

/* vim:set expandtab: */
