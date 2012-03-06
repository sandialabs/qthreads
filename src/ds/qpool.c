#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Internal Headers */
#include "qthread/qpool.h"
#include "qt_mpool.h"

qpool *qpool_create_aligned(const size_t isize,
                            size_t       alignment)
{                                      /*{{{ */
    return qt_mpool_create_aligned(isize, alignment);
}                                      /*}}} */

qpool *qpool_create(const size_t item_size)
{                                      /*{{{ */
    return qt_mpool_create_aligned(item_size, 0);
}                                      /*}}} */

void *qpool_alloc(qpool *pool)
{
    return qt_mpool_alloc(pool);
}

void qpool_free(qpool *restrict pool,
                void *restrict  mem)
{
    qt_mpool_free(pool, mem);
}

void qpool_destroy(qpool *pool)
{
    qt_mpool_destroy(pool);
}

/* vim:set expandtab: */
