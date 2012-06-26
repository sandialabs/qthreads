#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h> /* for malloc() */
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
# include <malloc.h> /* for memalign() */
#endif

#include <qthread/qthread-int.h> /* for uintptr_t */

#include "qt_visibility.h"
#include "qt_aligned_alloc.h"
#include "qthread_asserts.h"

void INTERNAL *qthread_internal_aligned_alloc(size_t         alloc_size,
                                              unsigned short alignment)
{
    void *ret;

    assert(alloc_size > 0);
    switch (alignment) {
        case 0:
            ret = malloc(alloc_size);
            break;
        default:
#if defined(HAVE_MEMALIGN)
            ret = memalign(alignment, alloc_size);
#elif defined(HAVE_POSIX_MEMALIGN)
            posix_memalign(&(ret), alignment, alloc_size);
#else
            {
                uint8_t *tmp = malloc((size + alignment - 1) + sizeof(void *));
                if (!tmp) { return NULL; }
                ret                 = (void *)(((uintptr_t)(tmp + sizeof(void *) + alignment - 1)) & ~(alignment - 1));
                *((void **)ret - 1) = tmp;
            }
            break;
#endif  /* if defined(HAVE_MEMALIGN) */
    }
    assert(((uintptr_t)ret & (alignment - 1)) == 0);
    return ret;
}

void INTERNAL qthread_internal_aligned_free(void *ptr)
{
    assert(ptr);
#if defined(HAVE_MEMALIGN) || defined(HAVE_POSIX_MEMALIGN)
    free(ptr);
#else
    free(*((void **)ptr - 1));
#endif
}

/* vim:set expandtab: */
