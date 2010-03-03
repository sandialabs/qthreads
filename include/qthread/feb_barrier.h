#ifndef QT_FEB_BARRIER_H
#define QT_FEB_BARRIER_H

#include <qthread/qthread.h>

typedef struct qt_feb_barrier_s qt_feb_barrier_t;

qt_feb_barrier_t *qt_feb_barrier_create(qthread_t * me, size_t max_threads);
void qt_feb_barrier_enter(qthread_t * me, qt_feb_barrier_t * b);
void qt_feb_barrier_destroy(qthread_t *me, qt_feb_barrier_t * b);

#endif
