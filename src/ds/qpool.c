#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#ifdef UNPOOLED
#include <stdlib.h> /* for malloc() */
#endif

/* Internal Headers */
#include "qthread/qpool.h"
#include "qt_mpool.h"
#include "qthread_asserts.h"

#undef UNPOOLED

#ifdef UNPOOLED
struct qt_mpool_s {
    size_t size;
    size_t alignment;
};
#endif

qpool *qpool_create_aligned(const size_t isize,
                            size_t       alignment)
{                                      /*{{{ */
#ifdef UNPOOLED
    qpool *ret = malloc(sizeof(struct qt_mpool_s));
    assert(ret);
    ret->size = isize;
    ret->alignment = alignment;
    return ret;
#else
    return qt_mpool_create_aligned(isize, alignment);
#endif
}                                      /*}}} */

qpool *qpool_create(const size_t item_size)
{                                      /*{{{ */
    return qpool_create_aligned(item_size, 0);
}                                      /*}}} */

void *qpool_alloc(qpool *pool)
{
#ifdef UNPOOLED
    return malloc(pool->size);
#else
    return qt_mpool_alloc(pool);
#endif
}

void qpool_free(qpool *restrict pool,
                void *restrict  mem)
{
#ifdef UNPOOLED
    free(mem);
#else
    qt_mpool_free(pool, mem);
#endif
}

void qpool_destroy(qpool *pool)
{
#ifdef UNPOOLED
    free(pool);
#else
    qt_mpool_destroy(pool);
#endif
}

/* vim:set expandtab: */
