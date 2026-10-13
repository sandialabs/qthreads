#ifndef QT_ALIGNED_ALLOC_H
#define QT_ALIGNED_ALLOC_H

#ifdef _WIN32
#include <malloc.h>
#define QT_ALIGNED_ALLOC(a, s) _aligned_malloc((s), (a))
#define QT_ALIGNED_FREE(p) _aligned_free(p)
#else
#include <stdlib.h>
#define QT_ALIGNED_ALLOC(a, s) aligned_alloc((a), (s))
#define QT_ALIGNED_FREE(p) free(p)
#endif

#endif
