#ifndef QT_MPOOL_H
#define QT_MPOOL_H

#include <stddef.h>		       /* for size_t (according to C89) */

typedef struct qt_mpool_s *qt_mpool;

void *qt_mpool_alloc(qt_mpool pool);

void qt_mpool_free(qt_mpool pool, void *mem);

// sync means pthread-protected
// allocation_bytes is how many bytes to return
// ...memory is always allocated in multiples of getpagesize()
qt_mpool qt_mpool_create(int sync, size_t allocation_bytes);

qt_mpool qt_mpool_create_aligned(int sync, size_t allocation_bytes,
				 size_t alignment);
void qt_mpool_destroy(qt_mpool pool);

#endif
