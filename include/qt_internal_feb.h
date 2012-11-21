#ifndef QTHREAD_INTERNAL_FEB
#define QTHREAD_INTERNAL_FEB

#include "qt_visibility.h"

typedef void (*qt_feb_callback_f)(void *addr, qthread_f f, void *arg, void *retloc, unsigned int thread_id, void *tls, void *callarg);

void INTERNAL qt_feb_subsystem_init(uint_fast8_t);

int INTERNAL qthread_writeEF_nb(aligned_t *restrict const       dest,
                                const aligned_t *restrict const src);
int INTERNAL qthread_writeEF_const_nb(aligned_t *const dest,
                                      const aligned_t  src);
int INTERNAL qthread_readFF_nb(aligned_t *restrict const       dest,
                               const aligned_t *restrict const src);
int INTERNAL qthread_readFE_nb(aligned_t *restrict const       dest,
                               const aligned_t *restrict const src);
int INTERNAL qthread_check_feb_preconds(qthread_t *t);

void INTERNAL qthread_feb_callback(qt_feb_callback_f cb,
                                   void             *arg);

#endif // ifndef QTHREAD_INTERNAL_FEB
/* vim:set expandtab: */
