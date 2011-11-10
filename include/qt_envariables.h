#ifndef QT_ENVARIABLES_H
#define QT_ENVARIABLES_H

#include "qt_visibility.h"

unsigned long INTERNAL qt_internal_get_env_num(const char   *envariable,
                                               unsigned long dflt,
                                               unsigned long zerodflt);

#endif
/* vim:set expandtab: */
