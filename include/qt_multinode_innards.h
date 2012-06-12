#ifndef QT_MULTINODE_INNARDS_H
#define QT_MULTINODE_INNARDS_H

#include "qt_visibility.h"

int INTERNAL qthread_multinode_initialize(void);
int INTERNAL qthread_multinode_register(uint32_t uid, qthread_f f);

#endif /* #ifndef QT_MULTINODE_INNARDS_H */
