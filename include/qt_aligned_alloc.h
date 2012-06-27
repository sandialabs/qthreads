#ifndef HAVE_QT_ALIGNED_ALLOC_H

#include "qt_visibility.h"

void INTERNAL *qthread_internal_aligned_alloc(size_t        alloc_size,
                                              uint_fast16_t alignment);
void INTERNAL qthread_internal_aligned_free(void         *ptr,
                                            uint_fast16_t alignment);

size_t INTERNAL qt_getpagesize(void);
#endif // ifndef HAVE_QT_ALIGNED_ALLOC_H
/* vim:set expandtab: */
