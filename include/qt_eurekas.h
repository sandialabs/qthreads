#ifndef QT_EUREKAS_H
#define QT_EUREKAS_H

#include "qt_visibility.h"

void INTERNAL qt_eureka_shepherd_init(void);
void INTERNAL qt_eureka_end_criticalsect_dead(qthread_t *self);
void INTERNAL qt_eureka_check(int block);

#endif
