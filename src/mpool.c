/* System Includes */
#include <pthread.h>

#include <stddef.h> /* for size_t (according to C89) */
#include <stdint.h>
#include <stdlib.h> /* for calloc() and malloc() */
#include <string.h>

/* External Headers */
#ifdef QTHREAD_USE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_DESTROY_MEMPOOL(a)
#define VALGRIND_MAKE_MEM_NOACCESS(a, b)
#define VALGRIND_MAKE_MEM_DEFINED(a, b)
#define VALGRIND_MAKE_MEM_UNDEFINED(a, b)
#define VALGRIND_CREATE_MEMPOOL(a, b, c)
#define VALGRIND_MEMPOOL_ALLOC(a, b, c)
#define VALGRIND_MEMPOOL_FREE(a, b)
#endif

/* Internal Includes */
#include "qt_alloc.h"
#include "qt_asserts.h"
#include "qt_atomics.h"
#include "qt_envariables.h"
#include "qt_expect.h"
#include "qt_gcd.h" /* for qt_lcm() */
#include "qt_macros.h"
#include "qt_mpool.h"
#include "qt_subsystems.h"
#include "qt_visibility.h"

/* Seems SLIGHTLY faster without TLS, and a whole lot safer and cleaner */
#ifdef TLS
#undef TLS
#endif

typedef struct threadlocal_cache_s qt_mpool_threadlocal_cache_t;

#ifdef TLS
static TLS_DECL_INIT(qt_mpool_threadlocal_cache_t *, pool_caches);
static TLS_DECL_INIT(uintptr_t, pool_cache_count);
static aligned_t pool_cache_global_max = 1;
static qt_mpool_threadlocal_cache_t **pool_cache_array = NULL;
#endif

struct qt_mpool_s {
  size_t item_size;
  size_t alloc_size;
  size_t items_per_alloc;
  size_t alignment;

#ifdef TLS
  size_t offset;
#else
  pthread_key_t threadlocal_cache;
#endif
  qt_mpool_threadlocal_cache_t *_Atomic caches; // for cleanup

  QTHREAD_FASTLOCK_TYPE reuse_lock;
  void *_Atomic reuse_pool;

  QTHREAD_FASTLOCK_TYPE pool_lock;
  void **alloc_list;
  size_t _Atomic alloc_list_pos;
};

typedef struct qt_mpool_cache_entry_s {
  struct qt_mpool_cache_entry_s *_Atomic next;
  struct qt_mpool_cache_entry_s *_Atomic block_tail;
  uint8_t data[];
} qt_mpool_cache_t;

struct threadlocal_cache_s {
  qt_mpool_cache_t *cache;
  uint_fast16_t count;
  uint8_t *block;
  uint_fast32_t i;
  qt_mpool_threadlocal_cache_t *_Atomic next; // for cleanup
};

#ifdef TLS
static void qt_mpool_subsystem_shutdown(void) {
  TLS_DELETE(pool_caches);
  TLS_DELETE(pool_cache_count);
  for (int i = 0; i < qthread_readstate(TOTAL_WORKERS); ++i) {
    if (pool_cache_array[i]) { qt_free(pool_cache_array[i]); }
  }
  FREE(pool_cache_array, sizeof(void *) * qthread_readstate(TOTAL_WORKERS));
}
#endif /* ifdef TLS */

void INTERNAL qt_mpool_subsystem_init(void) {
#ifdef TLS
  assert(TLS_GET(pool_caches) == NULL);
  assert(TLS_GET(pool_cache_count) == 0);
  pool_cache_array =
    qt_calloc(sizeof(void *), qthread_readstate(TOTAL_WORKERS));
  qthread_internal_cleanup_late(qt_mpool_subsystem_shutdown);
#endif
}

/* local funcs */
static inline void *qt_mpool_internal_aligned_alloc(size_t alloc_size,
                                                    size_t alignment) {
  void *ret = qt_internal_aligned_alloc(alloc_size, alignment);

  VALGRIND_MAKE_MEM_NOACCESS(ret, alloc_size);
  return ret;
}

static inline void qt_mpool_internal_aligned_free(void *freeme,
                                                  size_t alignment) {
  qt_internal_aligned_free(freeme, alignment);
}

// sync means lock-protected
// item_size is how many bytes to return
// ...memory is always allocated in multiples of getpagesize()
qt_mpool INTERNAL qt_mpool_create_aligned(size_t item_size, size_t alignment) {
  qt_mpool pool = (qt_mpool)MALLOC(sizeof(struct qt_mpool_s));

  size_t alloc_size = 0;
  /* Allow a user to specify a max_alloc_size. If no limit was specified
   * default to SIZE_MAX. Note that max_alloc_size will be bumped to at least
   * item_size * 2 after item_size has been adjusted for alignment, so that
   * at least two items can be allocated. */
  static size_t max_alloc_size = 0;
  if (max_alloc_size == 0) {
    max_alloc_size =
      qt_internal_get_env_num("MAX_POOL_ALLOC_SIZE", SIZE_MAX, 0);
  }

  qassert_ret((pool != NULL), NULL);
  VALGRIND_CREATE_MEMPOOL(pool, 0, 0);
  /* first, we ensure that item_size is at least sizeof(qt_mpool_cache_t), and
   * also that it is a multiple of sizeof(void*). */
  if (item_size < sizeof(qt_mpool_cache_t)) {
    item_size = sizeof(qt_mpool_cache_t);
  }
  if (item_size % sizeof(void *)) {
    item_size += (sizeof(void *)) - (item_size % sizeof(void *));
  }
  if (alignment <= 16) { alignment = 16; }
  if (item_size % alignment) {
    item_size += alignment - (item_size % alignment);
  }
  if (item_size * 2 >= max_alloc_size) { max_alloc_size = item_size * 2; }

  pool->item_size = item_size;
  pool->alignment = alignment;
  /* next, we find the least-common-multiple in sizes between item_size and
   * pagesize. If this is less than 128 items (an arbitrary number), we
   * increase the alloc_size until it is at least that big. This guarantees
   * that the allocation size will be a multiple of pagesize (fast!
   * efficient!). */
  alloc_size = qt_lcm(item_size, pagesize);

  if (alloc_size > max_alloc_size) {
    size_t max_num_items;
    /* adjust item_size to be a multiple of pagesize */
    item_size += pagesize - (item_size % pagesize);
    max_num_items = max_alloc_size / item_size; /* floor */
    if (max_num_items < 2) { max_num_items = 2; }
    alloc_size = item_size * max_num_items;
  }

  if (alloc_size == 0) { /* degenerative case */
    if (item_size > pagesize) {
      alloc_size = item_size;
    } else {
      alloc_size = pagesize;
    }
  } else {
    while ((alloc_size / item_size < 128) &&
           (alloc_size <= max_alloc_size / 2)) {
      alloc_size *= 2;
    }
    while (alloc_size < pagesize * 16) { alloc_size *= 2; }
  }
  QTHREAD_FASTLOCK_INIT(pool->reuse_lock);
  QTHREAD_FASTLOCK_INIT(pool->pool_lock);
  QTHREAD_FASTLOCK_LOCK(&pool->reuse_lock);
  atomic_store_explicit(&pool->reuse_pool, NULL, memory_order_relaxed);
  QTHREAD_FASTLOCK_UNLOCK(&pool->reuse_lock);
  QTHREAD_FASTLOCK_LOCK(&pool->pool_lock);
  pool->alloc_size = alloc_size;
  pool->items_per_alloc = alloc_size / item_size;
#ifdef TLS
  pool->offset = qthread_incr(&pool_cache_global_max, 1);
#else
  pthread_key_create(&pool->threadlocal_cache, NULL);
#endif
  /* this assumes that pagesize is a multiple of sizeof(void*) */
  assert(pagesize % sizeof(void *) == 0);
  pool->alloc_list = qt_internal_aligned_alloc(pagesize, pagesize);
  qassert_goto((pool->alloc_list != NULL), errexit);
  memset(pool->alloc_list, 0, pagesize);
  atomic_store_explicit(&pool->alloc_list_pos, 0u, memory_order_relaxed);

  atomic_store_explicit(&pool->caches, NULL, memory_order_relaxed);
  QTHREAD_FASTLOCK_UNLOCK(&pool->pool_lock);
  return pool;

  qgoto(errexit);
  if (pool) { FREE(pool, sizeof(struct qt_mpool_s)); }
  return NULL;
}

static qt_mpool_threadlocal_cache_t *qt_mpool_internal_getcache(qt_mpool pool) {
  qt_mpool_threadlocal_cache_t *tc;

#ifdef TLS
  tc = TLS_GET(pool_caches);
  {
    uintptr_t count_caches = (uintptr_t)TLS_GET(pool_cache_count);
    if (count_caches < pool->offset) {
#if !defined(NDEBUG)
      qthread_worker_id_t wkr = qthread_readstate(CURRENT_UNIQUE_WORKER);
      /* I don't fully understand why this is necessary. I *suspect* that
       * on thread 0, the initialization routine isn't happening
       * properly, so pool_caches gets a bogus value. However, that makes
       * no sense to me. */
      if ((wkr == 0) && (pool_cache_array[0] == NULL) && (tc != NULL)) {
        tc = NULL;
      }
#endif
      ASSERT_ONLY(
        if (wkr != NO_WORKER) { assert(pool_cache_array[wkr] == tc); })
      qt_mpool_threadlocal_cache_t *newtc =
        qt_realloc(tc, sizeof(qt_mpool_threadlocal_cache_t) * pool->offset);
      assert(newtc);
      if (tc != newtc) {
        qthread_worker_id_t wkr = qthread_readstate(CURRENT_UNIQUE_WORKER);
        tc = newtc;
        TLS_SET(pool_caches, newtc);
        if (wkr != NO_WORKER) { pool_cache_array[wkr] = newtc; }
      }
      memset(tc + count_caches,
             0,
             sizeof(qt_mpool_threadlocal_cache_t) *
               (pool->offset - count_caches));
      count_caches = pool->offset;
      TLS_SET(pool_cache_count, count_caches);
    } else if (tc == NULL) {
#if !defined(NDEBUG)
      qthread_worker_id_t wkr = qthread_readstate(CURRENT_UNIQUE_WORKER);
      /* I don't fully understand why this is necessary. I *suspect* that
       * on thread 0, the initialization routine isn't happening
       * properly, so pool_caches gets a bogus value. However, that makes
       * no sense to me. */
      if ((wkr == 0) && (pool_cache_array[0] == NULL) && (tc != NULL)) {
        tc = NULL;
      }
      if (wkr != NO_WORKER) { assert(pool_cache_array[wkr] == tc); }
#endif
    }
  }
  assert(tc);
  tc += (pool->offset - 1);
#else  /* ifdef TLS */
  tc = pthread_getspecific(pool->threadlocal_cache);
  if (NULL == tc) {
    tc = qt_internal_aligned_alloc(sizeof(qt_mpool_threadlocal_cache_t),
                                   CACHELINE_WIDTH);
    assert(tc);
    tc->cache = NULL;
    tc->count = 0;
    tc->block = NULL;
    tc->i = 0;
    qt_mpool_threadlocal_cache_t *old_caches;
    do {
      old_caches = atomic_load_explicit(&pool->caches, memory_order_relaxed);
      atomic_store_explicit(&tc->next, old_caches, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(&pool->caches,
                                                    &old_caches,
                                                    tc,
                                                    memory_order_release,
                                                    memory_order_relaxed));
    pthread_setspecific(pool->threadlocal_cache, tc);
  }
#endif /* ifdef TLS */
  return tc;
}

void INTERNAL *qt_mpool_alloc(qt_mpool pool) {
  qt_mpool_threadlocal_cache_t *tc;
  size_t cnt;

  qassert_ret((pool != NULL), NULL);

  tc = qt_mpool_internal_getcache(pool);
  if (tc->cache) {
    qt_mpool_cache_t *cache = tc->cache;
    tc->cache = atomic_load_explicit(&cache->next, memory_order_relaxed);
    --tc->count;
    return cache;
  } else if (tc->block) {
    void *ret = &(tc->block[tc->i * pool->item_size]);
    if (++tc->i == pool->items_per_alloc) { tc->block = NULL; }
    return ret;
  } else {
    size_t const items_per_alloc = pool->items_per_alloc;
    qt_mpool_cache_t *cache = NULL;

    cnt = 0;
    /* cache is empty; need to fill it */
    if (atomic_load_explicit(&pool->reuse_pool,
                             memory_order_relaxed)) { // global cache
      QTHREAD_FASTLOCK_LOCK(&pool->reuse_lock);
      if (atomic_load_explicit(&pool->reuse_pool, memory_order_relaxed)) {
        cache = atomic_load_explicit(&pool->reuse_pool, memory_order_relaxed);
        atomic_store_explicit(
          &pool->reuse_pool,
          atomic_load_explicit(&cache->block_tail->next, memory_order_relaxed),
          memory_order_relaxed);
        atomic_store_explicit(
          &cache->block_tail->next, NULL, memory_order_relaxed);
        cnt = items_per_alloc;
      }
      QTHREAD_FASTLOCK_UNLOCK(&pool->reuse_lock);
    }
    if (NULL == cache) {
      uint8_t *p;
      // const size_t item_size = pool->item_size;

      /* need to allocate a new block and record that I did so in the central
       * pool */
      p = qt_mpool_internal_aligned_alloc(pool->alloc_size, pool->alignment);
      qassert_ret((p != NULL), NULL);
      assert(pool->alignment == 0 ||
             (((uintptr_t)p) & (pool->alignment - 1)) == 0);
      QTHREAD_FASTLOCK_LOCK(&pool->pool_lock);
      if (atomic_load_explicit(&pool->alloc_list_pos, memory_order_relaxed) ==
          (pagesize / sizeof(void *) - 1)) {
        void **tmp = qt_internal_aligned_alloc(pagesize, pagesize);
        qassert_ret((tmp != NULL), NULL);
        memset(tmp, 0, pagesize);
        tmp[pagesize / sizeof(void *) - 1] = pool->alloc_list;
        pool->alloc_list = tmp;
        atomic_store_explicit(&pool->alloc_list_pos, 0, memory_order_relaxed);
      }
      pool->alloc_list[atomic_load_explicit(&pool->alloc_list_pos,
                                            memory_order_relaxed)] = p;
      atomic_fetch_add_explicit(
        &pool->alloc_list_pos, 1u, memory_order_relaxed);
      QTHREAD_FASTLOCK_UNLOCK(&pool->pool_lock);
      /* store the block for later allocation */
      tc->block = p;
      tc->i = 1;
      return p;
    } else {
      tc->cache = atomic_load_explicit(&cache->next, memory_order_relaxed);
      tc->count = cnt - 1;
      // cache->next       = NULL; // unnecessary
      // cache->block_tail = NULL; // unnecessary
      return cache;
    }
  }
}

void INTERNAL qt_mpool_free(qt_mpool pool, void *mem) {
  qt_mpool_threadlocal_cache_t *tc;
  qt_mpool_cache_t *cache = NULL;
  qt_mpool_cache_t *n = (qt_mpool_cache_t *)mem;
  size_t cnt;
  size_t const items_per_alloc = pool->items_per_alloc;

  qassert_retvoid((mem != NULL));
  qassert_retvoid((pool != NULL));
  tc = qt_mpool_internal_getcache(pool);
  cache = tc->cache;
  cnt = tc->count;
  if (cache) {
    assert(cnt != 0);
    atomic_store_explicit(&n->next, cache, memory_order_relaxed);
    atomic_store_explicit(
      &n->block_tail,
      atomic_load_explicit(&cache->block_tail, memory_order_relaxed),
      memory_order_relaxed); // cache is likely to be IN cache, so this won't be
                             // slow
  } else {
    assert(cnt == 0);
    atomic_store_explicit(&n->next, NULL, memory_order_relaxed);
    atomic_store_explicit(&n->block_tail, n, memory_order_relaxed);
  }
  cnt++;
  if (cnt >= (items_per_alloc * 2)) {
    qt_mpool_cache_t *toglobal;
    /* push to global */
    assert(n);
    assert(atomic_load_explicit(&n->block_tail, memory_order_relaxed));
    toglobal = atomic_load_explicit(
      &atomic_load_explicit(&n->block_tail, memory_order_relaxed)->next,
      memory_order_relaxed);
    atomic_store_explicit(
      &atomic_load_explicit(&n->block_tail, memory_order_relaxed)->next,
      NULL,
      memory_order_relaxed);
    assert(toglobal);
    assert(atomic_load_explicit(&toglobal->block_tail, memory_order_relaxed));
    QTHREAD_FASTLOCK_LOCK(&pool->reuse_lock);
    atomic_store_explicit(
      &atomic_load_explicit(&toglobal->block_tail, memory_order_relaxed)->next,
      atomic_load_explicit(&pool->reuse_pool, memory_order_relaxed),
      memory_order_relaxed);
    atomic_store_explicit(&pool->reuse_pool, toglobal, memory_order_relaxed);
    QTHREAD_FASTLOCK_UNLOCK(&pool->reuse_lock);
    cnt -= items_per_alloc;
  } else if (cnt == items_per_alloc + 1) {
    atomic_store_explicit(&n->block_tail, n, memory_order_relaxed);
  }
  tc->cache = n;
  tc->count = cnt;
  VALGRIND_MEMPOOL_FREE(pool, mem);
}

void INTERNAL qt_mpool_destroy(qt_mpool pool) {
  qassert_retvoid((pool != NULL));
  while (pool->alloc_list) {
    unsigned int i = 0;

    void *p = pool->alloc_list[0];

    while (p && i < (pagesize / sizeof(void *) - 1)) {
      qt_mpool_internal_aligned_free(p, pool->alignment);
      i++;
      p = pool->alloc_list[i];
    }
    p = pool->alloc_list;
    pool->alloc_list = pool->alloc_list[pagesize / sizeof(void *) - 1];
    qt_internal_aligned_free(p, pagesize);
  }
  qt_mpool_threadlocal_cache_t *freeme;
  while ((freeme = atomic_load_explicit(&pool->caches, memory_order_relaxed))) {
    atomic_store_explicit(
      &pool->caches,
      atomic_load_explicit(&freeme->next, memory_order_relaxed),
      memory_order_relaxed);
    qt_internal_aligned_free(freeme, CACHELINE_WIDTH);
  }
#ifndef TLS
  pthread_key_delete(pool->threadlocal_cache);
#endif
  QTHREAD_FASTLOCK_DESTROY(pool->pool_lock);
  QTHREAD_FASTLOCK_DESTROY(pool->reuse_lock);
  VALGRIND_DESTROY_MEMPOOL(pool);
  FREE(pool, sizeof(struct qt_mpool_s));
}

/* vim:set expandtab: */
