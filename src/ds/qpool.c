#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Internal Headers */
#include "qt_asserts.h"
#include "qt_mpool.h"
#include "qthread/qpool.h"
#ifdef UNPOOLED
#include "qt_debug.h" /* for malloc() debug wrappers */
#endif

#undef UNPOOLED

#ifdef UNPOOLED
struct qt_mpool_s {
  size_t size;
  size_t alignment;
};
#endif

qpool *qpool_create_aligned(size_t const isize, size_t alignment) { /*{{{ */
#ifdef UNPOOLED
  qpool *ret = MALLOC(sizeof(struct qt_mpool_s));
  assert(ret);
  ret->size = isize;
  ret->alignment = alignment;
  return ret;
#else
  return qt_mpool_create_aligned(isize, alignment);
#endif
} /*}}} */

qpool *qpool_create(size_t const item_size) { /*{{{ */
  return qpool_create_aligned(item_size, 0);
} /*}}} */

void *qpool_alloc(qpool *pool) {
#ifdef UNPOOLED
  return MALLOC(pool->size);
#else
  return qt_mpool_alloc(pool);
#endif
}

void qpool_free(qpool *restrict pool, void *restrict mem) {
#ifdef UNPOOLED
  FREE(mem, pool->size);
#else
  qt_mpool_free(pool, mem);
#endif
}

void qpool_destroy(qpool *pool) {
#ifdef UNPOOLED
  FREE(pool, sizeof(struct qt_mpool_s));
#else
  qt_mpool_destroy(pool);
#endif
}

/* vim:set expandtab: */
