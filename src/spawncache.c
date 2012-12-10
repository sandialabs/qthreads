#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <pthread.h> /* for pthread_key_*() */
#include <string.h>  /* for memset() */

/* Public Headers */
#include "qthread/cacheline.h"

/* Internal Headers */
#include "qt_spawncache.h"
#include "qt_visibility.h"
#include "qt_debug.h"
#include "qt_asserts.h"
#include "qt_macros.h"
#include "qt_aligned_alloc.h"
#include "qt_subsystems.h"

/* Globals */
TLS_DECL_INIT(qt_threadqueue_private_t *, spawn_cache);

/* Static Functions */
static void qt_spawncache_shutdown(void)
{
    qthread_debug(CORE_DETAILS, "destroy thread-local task queue\n");
    void *freeme = TLS_GET(spawn_cache);
    qthread_internal_aligned_free(freeme, qthread_cacheline());
    TLS_DELETE(spawn_cache);
}

#ifndef TLS
static void qt_threadqueue_private_destroy(void *q)
{
    assert(((qt_threadqueue_private_t *)q)->head == NULL &&
           ((qt_threadqueue_private_t *)q)->tail == NULL &&
           ((qt_threadqueue_private_t *)q)->qlength == 0 &&
           ((qt_threadqueue_private_t *)q)->qlength_stealable == 0);

    qthread_internal_aligned_free(q, qthread_cacheline());
}

#endif /* ifndef TLS */

/* Internal-only Functions */
void INTERNAL qt_spawncache_init(void)
{
    TLS_INIT2(spawn_cache, qt_threadqueue_private_destroy);
    qthread_internal_cleanup(qt_spawncache_shutdown);
}

qt_threadqueue_private_t INTERNAL *qt_init_local_spawncache(void)
{
    void *const ret = qthread_internal_aligned_alloc(sizeof(qt_threadqueue_private_t), qthread_cacheline());

    assert(ret);
    memset(ret, 0, sizeof(qt_threadqueue_private_t));

    TLS_SET(spawn_cache, ret);
    return (qt_threadqueue_private_t *)ret;
}

qt_threadqueue_private_t INTERNAL *qt_spawncache_get()
{
    return TLS_GET(spawn_cache);
}

int INTERNAL qt_spawncache_spawn(qthread_t        *t,
                                 qt_threadqueue_t *q)
{
    qt_threadqueue_private_t *cache = TLS_GET(spawn_cache);

    if (cache) {
        int ret = qt_threadqueue_private_enqueue(cache, q, t);
        if( !ret) {
            return ret;
        }
        return ret;
    } else {
        return 0;
    }
}

int INTERNAL qt_spawncache_yield(qthread_t *t)
{
    qt_threadqueue_private_t *cache = TLS_GET(spawn_cache);

    if (cache) {
        return qt_threadqueue_private_enqueue_yielded(cache, t);
    } else {
        return 0;
    }
}

void INTERNAL qt_spawncache_filter(qt_threadqueue_filter_f f)
{
    qt_threadqueue_private_t *cache = TLS_GET(spawn_cache);

    if (cache)
        qt_threadqueue_private_filter(cache, f);
}


int INTERNAL qt_spawncache_flush(qt_threadqueue_t *q)
{
    qt_threadqueue_private_t *cache = TLS_GET(spawn_cache);

    if (cache) {
        qthread_debug(THREADQUEUE_BEHAVIOR, "attempting to flush cache %p\n", cache);
        if (cache->on_deck) {
            qthread_debug(THREADQUEUE_DETAILS, "attempting to flush cache %p into queue %p : %u items\n", cache, q, cache->qlength + 1);
            qt_threadqueue_enqueue_cache(q, cache);
            return 1;
        }
        return 0;
    } else {
        return 0;
    }
}

/* vim:set expandtab: */
