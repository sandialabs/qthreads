#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>		       /* debugging */
#include <qthread/qthread.h>
#include <qthread/qpool.h>
#include <stddef.h>		       /* for size_t (according to C89) */
#include <stdlib.h>		       /* for calloc() and malloc() */
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
#include <malloc.h>		       /* for memalign() */
#endif
#ifdef QTHREAD_HAVE_LIBNUMA
# include <numa.h>
#endif

#include "qthread_asserts.h"
#include "qt_atomics.h"

#ifdef HAVE_GETPAGESIZE
#include <unistd.h>
#else
# define getpagesize() 4096
#endif

struct qpool_s
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

    unsigned int node;
    aligned_t lock;
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

static QINLINE void *qpool_internal_aligned_alloc(size_t alloc_size,
						     unsigned int node,
						     size_t alignment)
{
#if QTHREAD_HAVE_LIBNUMA
    if (node == QTHREAD_NO_NODE) {		       // guaranteed page alignment
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

static QINLINE void qpool_internal_aligned_free(void *freeme,
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
	free(freeme, alloc_size);
    } else {
# ifdef HAVE_WORKING_VALLOC
	free(freeme);
# endif
	return;			       /* XXX: cannot necessarily free valloc'd memory */
    }
#endif
}

// item_size is how many bytes to return
// ...memory is always allocated in multiples of getpagesize()
qpool qpool_create_aligned(qthread_t *me, const size_t isize, const size_t alignment)
{
    qpool pool;
    size_t item_size = isize;
    size_t alloc_size = 0;

    assert(me != NULL);
    if (me == NULL) {
	return NULL;
    }
    pool = (qpool)calloc(1, sizeof(struct qpool_s));
    assert(pool != NULL);
    if (pool == NULL) {
	return NULL;
    }
    pool->node = qthread_internal_shep_to_node(qthread_shep(me));
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
	(char *)qpool_internal_aligned_alloc(alloc_size, pool->node, alignment);
    assert(alignment == 0 || ((unsigned long)(pool->alloc_block) & (alignment - 1)) == 0);
    assert(pool->alloc_block != NULL);
    if (pool->alloc_block == NULL) {
	free(pool);
	return NULL;
    }
    /* this assumes that pagesize is a multiple of sizeof(void*) */
    pool->alloc_list = calloc(1, pagesize);
    assert(pool->alloc_list != NULL);
    if (pool->alloc_list == NULL) {
	qpool_internal_aligned_free(pool->alloc_block, alloc_size,
				       alignment);
	free(pool);
	return NULL;
    }
    pool->alloc_list[0] = pool->alloc_block;
    pool->alloc_list_pos = 1;
    return pool;
}

qpool qpool_create(qthread_t *me, const size_t item_size)
{
    return qpool_create_aligned(me, item_size, 0);
}

void *qpool_alloc(qthread_t *me, qpool pool)
{
    void *p = (void *)(pool->reuse_pool);

    assert(pool);
    if (p) {
	void *old, *new;

	do {
	    old = p;
	    new = *(void **)p;
	    p = qt_cas(&(pool->reuse_pool), old, new);
	} while (p != old);
    }
    if (!p) {			       /* this is not an else on purpose */
	qassert(qthread_lock(me, &pool->lock), 0);
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
	    p = qpool_internal_aligned_alloc(pool->alloc_size, pool->node,
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
	qassert(qthread_unlock(me, &pool->lock), 0);
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

void qpool_free(qpool pool, void *mem)
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

void qpool_destroy(qpool pool)
{
    assert(pool);
    if (pool) {
	while (pool->alloc_list) {
	    unsigned int i = 0;

	    void *p = pool->alloc_list[0];

	    while (p && i < (pagesize / sizeof(void *) - 1)) {
		qpool_internal_aligned_free(p, pool->alloc_size,
					       pool->alignment);
		i++;
		p = pool->alloc_list[i];
	    }
	    p = pool->alloc_list;
	    pool->alloc_list =
		pool->alloc_list[pagesize / sizeof(void *) - 1];
	    free(p);
	}
	free(pool);
    }
}
