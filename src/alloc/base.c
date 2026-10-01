#include <stdint.h>
#include <stdlib.h>

/* System Headers */
#include <sys/mman.h> // mmap
#include <unistd.h>   /* for getpagesize() */

/* Internal Headers */
#include "qt_alloc.h"
#include "qt_asserts.h"

/* local constants */
size_t _pagesize = 0;

void *qt_malloc(size_t size) { return malloc(size); }

void qt_free(void *ptr) { free(ptr); }

void *qt_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }

void *qt_realloc(void *ptr, size_t size) { return realloc(ptr, size); }

void qt_internal_alignment_init(void) { _pagesize = getpagesize(); }

void *qt_internal_aligned_alloc(size_t alloc_size,
                                uint_fast16_t alignment_small) {
  size_t alignment = alignment_small;
  // round alloc_size up to the nearest multiple of alignment
  // since that's required by the standard aligned_alloc
  // and the implementation on OSX actually enforces that.
  if (alignment) {
    alloc_size = ((alloc_size + (alignment - 1ull)) / alignment) * alignment;
  }
  return aligned_alloc(alignment, alloc_size);
}

void qt_internal_aligned_free(void *ptr, uint_fast16_t alignment) {
  qt_free(ptr);
}

void *qt_internal_stack_alloc(size_t alloc_size) {
  // mmap returns a page-aligned address.
  // We're assumign that will always be larger than the stack alignment.
  return mmap(NULL,
              alloc_size,
              PROT_READ | PROT_WRITE,
#ifdef MAP_STACK
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
#else
              MAP_PRIVATE | MAP_ANONYMOUS,
#endif
              -1,
              0);
}

void qt_internal_stack_free(void *ptr, size_t alloc_size) {
#ifdef NDEBUG
  munmap(ptr, alloc_size);
#else
  int ret = munmap(ptr, alloc_size);
  assert(!ret);
#endif
}

/* vim:set expandtab: */
