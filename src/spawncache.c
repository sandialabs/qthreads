#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <pthread.h> /* for pthread_key_*() */
#include <string.h>  /* for memset() */
#include <stdlib.h>  /* for malloc() and free(), per C89 */
#ifdef HAVE_MALLOC_H
# include <malloc.h> /* for memalign() */
#endif

/* Internal Headers */
#include "qt_spawncache.h"
#include "qt_visibility.h"
#include "qt_debug.h"
#include "qthread_asserts.h"
#include "qthread_innards.h"
#include "qthread/cacheline.h"
#include "qt_macros.h"

/* Globals */
TLS_DECL(qt_threadqueue_private_t*,spawn_cache);

/* Static Functions */
static void qt_spawncache_shutdown(void)
{
    qthread_debug(CORE_DETAILS, "destroy thread-local task queue\n");
    void *freeme = TLS_GET(spawn_cache);
    free(freeme);
    TLS_DELETE(spawn_cache);
}

#ifndef TLS
static void qt_threadqueue_private_destroy(void *q)
{
    assert(((qt_threadqueue_private_t *)q)->head == NULL &&
           ((qt_threadqueue_private_t *)q)->tail == NULL &&
           ((qt_threadqueue_private_t *)q)->qlength == 0 &&
           ((qt_threadqueue_private_t *)q)->qlength_stealable == 0);

    free(q);
}
#endif

/* Internal-only Functions */
void INTERNAL qt_spawncache_init(void)
{
    TLS_INIT2(spawn_cache, qt_threadqueue_private_destroy);
    qthread_internal_cleanup(qt_spawncache_shutdown);
}

qt_threadqueue_private_t INTERNAL *qt_init_local_spawncache(void)
{
#ifdef HAVE_MEMALIGN
    void *const ret = memalign(qthread_cacheline(), sizeof(qt_threadqueue_private_t));
#elif defined(HAVE_POSIX_MEMALIGN)
    void *ret = NULL;
    posix_memalign(&(ret), qthread_cacheline(), sizeof(qt_threadqueue_private_t));
#elif defined(HAVE_WORKING_VALLOC)
    void *const ret = valloc(sizeof(qt_threadqueue_private_t));
#elif defined(HAVE_PAGE_ALIGNED_MALLOC)
    void *const ret = malloc(sizeof(qt_threadqueue_private_t));
#else
    void *const ret = valloc(sizeof(qt_threadqueue_private_t));  /* cross your fingers */
#endif
    memset(ret, 0, sizeof(qt_threadqueue_private_t));

    TLS_SET(spawn_cache, ret);
    return (qt_threadqueue_private_t *)ret;
}

int INTERNAL qt_spawncache_spawn(qthread_t *t)
{
    qt_threadqueue_private_t *cache = TLS_GET(spawn_cache);

    if (cache) {
        return qt_threadqueue_private_enqueue(cache, t);
    } else {
        return 0;
    }
}

/* vim:set expandtab: */
