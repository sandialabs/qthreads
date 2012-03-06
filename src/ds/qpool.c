#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>                     /* debugging */
#include <qthread/qthread.h>
#include <qthread/qpool.h>
#include <stddef.h>                    /* for size_t (according to C89) */
#include <stdlib.h>                    /* for calloc() and malloc() */
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
# include <malloc.h>                   /* for memalign() */
#endif
#ifdef QTHREAD_USE_VALGRIND
# include <valgrind/memcheck.h>
#else
# define VALGRIND_DESTROY_MEMPOOL(a)
# define VALGRIND_MAKE_MEM_NOACCESS(a, b)
# define VALGRIND_MAKE_MEM_DEFINED(a, b)
# define VALGRIND_CREATE_MEMPOOL(a, b, c)
# define VALGRIND_MEMPOOL_ALLOC(a, b, c)
# define VALGRIND_MEMPOOL_FREE(a, b)
#endif

#include "qthread_innards.h"           /* for QTHREAD_NO_NODE */
#include "qthread_asserts.h"
#include "qt_affinity.h"
#include "qt_gcd.h"                    /* for qt_lcm() */

#ifdef HAVE_GETPAGESIZE
# include <unistd.h>
#else
# define getpagesize() 4096
#endif

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

void qpool_free(qpool *pool, void *mem)
{
    qt_mpool_free(pool, mem);
}

void qpool_destroy(qpool * pool)
{
    qt_mpool_destroy(pool);
}

/* vim:set expandtab: */
