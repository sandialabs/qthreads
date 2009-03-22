#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>		       /* debugging */
#include "qt_mpool.h"
#include "qt_atomics.h"
#include <stddef.h>		       /* for size_t (according to C89) */
#include <stdlib.h>		       /* for calloc() and malloc() */
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
#include <malloc.h>		       /* for memalign() */
#endif
#ifdef QTHREAD_HAVE_LIBNUMA
# include <numa.h>
#endif

#ifdef QTHREAD_USE_PTHREADS
#include <pthread.h>
#endif

#include "qthread_asserts.h"

#ifdef HAVE_GETPAGESIZE
#include <unistd.h>
#else
static QINLINE int getpagesize()
{
    return 4096;
}
#endif

struct qt_mpool_s
{
    size_t item_size;
    size_t alloc_size;
    size_t items_per_alloc;
    size_t alignment;

    volatile void *reuse_pool;
    char *alloc_block;
    size_t alloc_block_pos;
    void **alloc_list;
    size_t alloc_list_pos;

    int node;
#ifdef QTHREAD_USE_PTHREADS
    pthread_mutex_t *lock;
#endif
};

/* local constants */
static size_t pagesize = 0;

/* local funcs */
static QINLINE size_t mpool_gcd(size_t a, size_t b)
{
    while (1) {
	if (a == 0)
	    return b;
	b %= a;
	if (b == 0)
	    return a;
	a %= b;
    }
}

static QINLINE size_t mpool_lcm(size_t a, size_t b)
{
    size_t tmp = mpool_gcd(a, b);

    return (tmp != 0) ? (a * b / tmp) : 0;
}

static QINLINE void *qt_mpool_internal_aligned_alloc(size_t alloc_size,
						     int node,
						     size_t alignment)
{
#if QTHREAD_HAVE_LIBNUMA
    if (node == -1) {		       // guaranteed page alignment
	return numa_alloc_interleaved(alloc_size);
    } else {
	return numa_alloc_onnode(alloc_size, node);
    }
#else
    switch (alignment) {
	case 0:
	    return malloc(alloc_size);
	case 16:
#ifdef HAVE_16ALIGNED_MALLOC
	case 8:
	case 4:
	case 2:
	    return malloc(alloc_size);
#elif HAVE_MEMALIGN
	    return memalign(16, alloc_size);
#elif HAVE_POSIX_MEMALIGN
	{
	    void *ret;

	    posix_memalign(&(ret), 16, alloc_size);
	    return ret;
	}
#elif HAVE_PAGE_ALIGNED_MALLOC
	    return malloc(alloc_size);
#else
	    return valloc(alloc_size);
#endif
	default:
#ifdef HAVE_MEMALIGN
	    return memalign(alignment, alloc_size);
#elif HAVE_POSIX_MEMALIGN
	{
	    void *ret;

	    posix_memalign(&(ret), alignment, alloc_size);
	    return ret;
	}
#elif HAVE_PAGE_ALIGNED_MALLOC
	    return malloc(alloc_size);
#else
	    return valloc(alloc_size); /* cross your fingers */
#endif
    }
#endif
}

static QINLINE void qt_mpool_internal_aligned_free(void *freeme,
						   const size_t alloc_size,
						   const size_t alignment)
{
#if QTHREAD_HAVE_LIBNUMA
    numa_free(freeme, alloc_size);
#elif (HAVE_MEMALIGN || HAVE_PAGE_ALIGNED_MALLOC || HAVE_POSIX_MEMALIGN)
    free(freeme);
#elif HAVE_16ALIGNED_MALLOC
    switch (alignment) {
	case 16:
	case 8:
	case 4:
	case 2:
	case 0:
	    free(freeme);
    }
#else
    if (alignment == 0) {
	free(freeme);
    } else {
# ifdef HAVE_WORKING_VALLOC
	free(freeme);
# endif
	return;
    }
#endif
}

// sync means pthread-protected
// item_size is how many bytes to return
// ...memory is always allocated in multiples of getpagesize()
qt_mpool qt_mpool_create_aligned(const int sync, size_t item_size,
				 const int node, const size_t alignment)
{
    qt_mpool pool = (qt_mpool)calloc(1, sizeof(struct qt_mpool_s));

    size_t alloc_size = 0;

    assert(pool != NULL);
    if (pool == NULL) {
	return NULL;
    }
    pool->node = node;
#ifdef QTHREAD_USE_PTHREADS
    if (sync) {
	pool->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	assert(pool->lock != NULL);
	if (pool->lock == NULL) {
	    free(pool);
	    return NULL;
	}
	if (pthread_mutex_init(pool->lock, NULL) != 0) {
	    assert(0);
	    free(pool->lock);
	    free(pool);
	    return NULL;
	}
    }
#endif
    if (pagesize == 0) {
	pagesize = getpagesize();
    }
    /* first, we ensure that item_size is at least sizeof(void*), and also that
     * it is a multiple of sizeof(void*). The second condition technically
     * implies the first, but it's not a big deal. */
    if (item_size < sizeof(void *)) {
	item_size = sizeof(void *);
    }
    if (item_size % sizeof(void *)) {
	item_size += (sizeof(void *)) - (item_size % sizeof(void *));
    }
    if (alignment != 0 && item_size % alignment) {
	item_size += alignment - (item_size % alignment);
    }
    pool->item_size = item_size;
    pool->alignment = alignment;
    /* next, we find the least-common-multiple in sizes between item_size and
     * pagesize. If this is less than ten items (an arbitrary number), we
     * increase the alloc_size until it is at least that big. This guarantees
     * that the allocation size will be a multiple of pagesize (fast!
     * efficient!). */
    alloc_size = mpool_lcm(item_size, pagesize);
    if (alloc_size == 0) {	       /* degenerative case */
	if (item_size > pagesize) {
	    alloc_size = item_size;
	} else {
	    alloc_size = pagesize;
	}
    } else {
	while (alloc_size / item_size < 128) {
	    alloc_size *= 2;
	}
	while (alloc_size < pagesize * 16) {
	    alloc_size *= 2;
	}
    }
    pool->alloc_size = alloc_size;

    pool->items_per_alloc = alloc_size / item_size;

    pool->reuse_pool = NULL;
    pool->alloc_block =
	(char *)qt_mpool_internal_aligned_alloc(alloc_size, node, alignment);
    assert(alignment == 0 || ((unsigned long)(pool->alloc_block) & (alignment - 1)) == 0);
    assert(pool->alloc_block != NULL);
    if (pool->alloc_block == NULL) {
#ifdef QTHREAD_USE_PTHREADS
	if (sync) {
	    qassert(pthread_mutex_destroy(pool->lock), 0);
	    free(pool->lock);
	}
#endif
	free(pool);
	return NULL;
    }
    /* this assumes that pagesize is a multiple of sizeof(void*) */
    pool->alloc_list = calloc(1, pagesize);
    assert(pool->alloc_list != NULL);
    if (pool->alloc_list == NULL) {
	qt_mpool_internal_aligned_free(pool->alloc_block, alloc_size,
				       alignment);
#ifdef QTHREAD_USE_PTHREADS
	if (sync) {
	    qassert(pthread_mutex_destroy(pool->lock), 0);
	    free(pool->lock);
	}
#endif
	free(pool);
	return NULL;
    }
    pool->alloc_list[0] = pool->alloc_block;
    pool->alloc_list_pos = 1;
    return pool;
}

qt_mpool qt_mpool_create(int sync, size_t item_size, int node)
{
    return qt_mpool_create_aligned(sync, item_size, node, 0);
}

void *qt_mpool_alloc(qt_mpool pool)
{
    void *p = (void *)(pool->reuse_pool);

    assert(pool);
    if (p) {
	void *old, *new;

	/* note that this is only "safe" as long as there is no chance that the
	 * pool has been destroyed. There's a small chance that p was allocated
	 * and popped back into the queue so that the CAS works but "new" is
	 * the wrong value... (also known among lock-free wonks as The ABA
	 * Problem) fixing it would require the QPTR stuff that qlfqueue uses.
	 * */
	do {
	    old = p;
	    new = *(void **)p;
	    p = qt_cas(&(pool->reuse_pool), old, new);
	} while (p != old);
    }
    if (!p) {			       /* this is not an else on purpose */
#ifdef QTHREAD_USE_PTHREADS
	if (pool->lock) {
	    qassert(pthread_mutex_lock(pool->lock), 0);
	}
#endif
	if (pool->alloc_block_pos == pool->items_per_alloc) {
	    if (pool->alloc_list_pos == (pagesize / sizeof(void *) - 1)) {
		void **tmp = calloc(1, pagesize);

		assert(tmp != NULL);
		if (tmp == NULL) {
		    goto alloc_exit;
		}
		tmp[pagesize / sizeof(void *) - 1] = pool->alloc_list;
		pool->alloc_list = tmp;
		pool->alloc_list_pos = 0;
	    }
	    p = qt_mpool_internal_aligned_alloc(pool->alloc_size, pool->node,
						pool->alignment);
	    assert(p != NULL);
	    assert(pool->alignment == 0 ||
		   (((unsigned long)p) & (pool->alignment - 1)) == 0);
	    if (p == NULL) {
		goto alloc_exit;
	    }
	    pool->alloc_block = p;
	    pool->alloc_block_pos = 1;
	    pool->alloc_list[pool->alloc_list_pos] = pool->alloc_block;
	    pool->alloc_list_pos++;
	} else {
	    p = pool->alloc_block + (pool->item_size * pool->alloc_block_pos);
	    pool->alloc_block_pos++;
	}
#ifdef QTHREAD_USE_PTHREADS
	if (pool->lock) {
	    qassert(pthread_mutex_unlock(pool->lock), 0);
	}
#endif
	if (pool->alignment != 0 &&
	    (((unsigned long)p) & (pool->alignment - 1))) {
	    printf("alloc_block = %p\n", pool->alloc_block);
	    printf("item_size = %u\n", (unsigned)(pool->item_size));
	    assert(pool->alignment == 0 ||
		   (((unsigned long)p) & (pool->alignment - 1)) == 0);
	}
    }
  alloc_exit:
    return p;
}

void qt_mpool_free(qt_mpool pool, void *mem)
{
    void *p, *old, *new;

    assert(mem != NULL);
    assert(pool);
    do {
	old = (void *)(pool->reuse_pool);	// should be an atomic read
	*(void **)mem = old;
	new = mem;
	p = qt_cas(&(pool->reuse_pool), old, new);
    } while (p != old);
}

void qt_mpool_destroy(qt_mpool pool)
{
    assert(pool);
    if (pool) {
	while (pool->alloc_list) {
	    unsigned int i = 0;

	    void *p = pool->alloc_list[0];

	    while (p && i < (pagesize / sizeof(void *) - 1)) {
		qt_mpool_internal_aligned_free(p, pool->alloc_size,
					       pool->alignment);
		i++;
		p = pool->alloc_list[i];
	    }
	    p = pool->alloc_list;
	    pool->alloc_list =
		pool->alloc_list[pagesize / sizeof(void *) - 1];
	    free(p);
	}
#ifdef QTHREAD_USE_PTHREADS
	if (pool->lock) {
	    qassert(pthread_mutex_destroy(pool->lock), 0);
	    free(pool->lock);
	}
#endif
	free(pool);
    }
}
