#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>
#include <stdio.h> /* debugging */
#include "qt_mpool.h"
#include <stddef.h> /* for size_t (according to C89) */
#include <stdlib.h> /* for calloc() and malloc() */

#ifdef QTHREAD_USE_PTHREADS
#include <pthread.h>
#endif

#ifdef HAVE_GETPAGESIZE
#include <unistd.h>
#else
static inline int getpagesize() { return 4096; }
#endif

struct qt_mpool_s {
    size_t item_size;
    size_t alloc_size;
    size_t items_per_alloc;

    char * reuse_pool;
    char * alloc_block;
    size_t alloc_block_pos;
    void ** alloc_list;
    size_t alloc_list_pos;

#ifdef QTHREAD_USE_PTHREADS
    pthread_mutex_t *lock;
#endif
};

/* local constants */
static size_t pagesize = 0;

/* local funcs */
static inline size_t mpool_gcd(size_t a, size_t b)
{
    while (1) {
	if (a == 0) return b;
	b %= a;
	if (b == 0) return a;
	a %= b;
    }
}
static inline size_t mpool_lcm(size_t a, size_t b)
{
    int tmp = mpool_gcd(a,b);
    return (tmp!=0)?(a*b/tmp):0;
}

// sync means pthread-protected
// item_size is how many bytes to return
// ...memory is always allocated in multiples of getpagesize()
qt_mpool qt_mpool_create(int sync, size_t item_size)
{
    qt_mpool pool = (qt_mpool) calloc(1, sizeof(struct qt_mpool_s));
    size_t alloc_size = 0;
    if (pool == NULL) {
	return NULL;
    }
#ifdef QTHREAD_USE_PTHREADS
    if (sync) {
	pool->lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	if (pool->lock == NULL) {
	    free(pool);
	    return NULL;
	}
	if (pthread_mutex_init(pool->lock, NULL) != 0) {
	    free(pool->lock);
	    free(pool);
	    return NULL;
	}
    }
#endif
    if (pagesize == 0) {
	pagesize = getpagesize();
    }
    /* first, we ensure that item_size is a multiple of WORD_SIZE, and also
     * that it is at least sizeof(void*). The first condition may imply the
     * second on *most* platforms, but costs us very little to make sure. */
    if (item_size < sizeof(void*)) {
	item_size = sizeof(void*);
    }
    if (item_size % sizeof(void*)) {
	item_size += (sizeof(void*)) - (item_size % sizeof(void*));
    }
    pool->item_size = item_size;
    /* next, we find the least-common-multiple in sizes between item_size and
     * pagesize. If this is less than ten items (an arbitrary number), we
     * increase the alloc_size until it is at least that big. This guarantees
     * that the allocation size will be a multiple of pagesize (fast!
     * efficient!). */
    alloc_size = mpool_lcm(item_size, pagesize);
    while (alloc_size/item_size < 10) {
	alloc_size *= 2;
    }
    pool->alloc_size = alloc_size;

    pool->items_per_alloc = alloc_size/item_size;

    pool->reuse_pool = NULL;
    pool->alloc_block = (char *) malloc(alloc_size);
    if (pool->alloc_block == NULL) {
#ifdef QTHREAD_USE_PTHREADS
	if (sync) {
	    pthread_mutex_destroy(pool->lock);
	    free(pool->lock);
	}
#endif
	free(pool);
	return NULL;
    }
    /* this assumes that pagesize is a multiple of sizeof(void*) */
    pool->alloc_list = calloc(1, pagesize);
    if (pool->alloc_list == NULL) {
	free(pool->alloc_block);
#ifdef QTHREAD_USE_PTHREADS
	if (sync) {
	    pthread_mutex_destroy(pool->lock);
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

void * qt_mpool_alloc(qt_mpool pool)
{
    void *p = NULL;

#ifdef QTHREAD_USE_PTHREADS
    if (pool->lock) {
	pthread_mutex_lock(pool->lock);
    }
#endif
    if (pool->reuse_pool) {
	p = pool->reuse_pool;
	pool->reuse_pool = *(void**)p;
    } else {
	if (pool->alloc_block_pos == pool->items_per_alloc) {
	    if (pool->alloc_list_pos == (pagesize/sizeof(void*) - 1)) {
		void ** tmp = calloc(1, pagesize);
		if (tmp == NULL) {
		    goto alloc_exit;
		}
		tmp[pagesize/sizeof(void*)-1] = pool->alloc_list;
		pool->alloc_list = tmp;
		pool->alloc_list_pos = 0;
	    }
	    p = malloc(pool->alloc_size);
	    if (p == NULL) {
		goto alloc_exit;
	    }
	    pool->alloc_block = p;
	    pool->alloc_block_pos = 1;
	    pool->alloc_list[pool->alloc_list_pos] = pool->alloc_block;
	    pool->alloc_list_pos ++;
	} else {
	    p = pool->alloc_block + (pool->item_size * pool->alloc_block_pos);
	    pool->alloc_block_pos ++;
	}
    }
alloc_exit:
#ifdef QTHREAD_USE_PTHREADS
    if (pool->lock) {
	pthread_mutex_unlock(pool->lock);
    }
#endif
    return p;
}

void qt_mpool_free(qt_mpool pool, void * mem)
{
#ifdef QTHREAD_USE_PTHREADS
    if (pool->lock) {
	pthread_mutex_lock(pool->lock);
    }
#endif
    *(void**) mem = pool->reuse_pool;
    pool->reuse_pool = mem;
#ifdef QTHREAD_USE_PTHREADS
    if (pool->lock) {
	pthread_mutex_unlock(pool->lock);
    }
#endif
}

void qt_mpool_destroy(qt_mpool pool)
{
    if (pool) {
	while (pool->alloc_list) {
	    unsigned int i=0;
	    void *p = pool->alloc_list[0];
	    while (p && i < (pagesize/sizeof(void*) - 1)) {
		free(p);
		i++;
		p = pool->alloc_list[i];
	    }
	    p = pool->alloc_list;
	    pool->alloc_list = pool->alloc_list[pagesize/sizeof(void*)-1];
	    free(p);
	}
#ifdef QTHREAD_USE_PTHREADS
	if (pool->lock) {
	    pthread_mutex_destroy(pool->lock);
	    free(pool->lock);
	}
#endif
	free(pool);
    }
}
