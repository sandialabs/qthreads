#ifndef HAVE_QT_ALIGNED_ALLOC_H

#include "qt_visibility.h"

void INTERNAL *qthread_internal_aligned_alloc(size_t         alloc_size,
                                              unsigned short alignment);
void INTERNAL qthread_internal_aligned_free(void *ptr);

#endif
/* vim:set expandtab: */
