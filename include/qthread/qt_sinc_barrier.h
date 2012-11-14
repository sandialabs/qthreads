#ifndef QT_SINC_BARRIER_H
#define QT_SINC_BARRIER_H

#include <qthread/sinc.h>
#include <qthread/macros.h>

Q_STARTCXX /* */

typedef struct qt_sinc_barrier_s {
    uint64_t   count;
    qt_sinc_t *sinc_1;
    qt_sinc_t *sinc_2;
    qt_sinc_t *sinc_3;
} qt_sinc_barrier_t;

void API_FUNC qt_sinc_barrier_enter(qt_sinc_barrier_t *b);
void          qt_sinc_barrier_init(qt_sinc_barrier_t *restrict barrier,
                                   size_t                      expect);
void qt_sinc_barrier_destroy(qt_sinc_barrier_t *restrict barrier);
void qt_sinc_barrier_change(qt_sinc_barrier_t *restrict barrier,
                            size_t                      expect);

/* functions definded in qthread.c to support nested parallelism and barriers */
qt_sinc_barrier_t *qt_get_barrier(void);
void               qt_set_barrier(qt_sinc_barrier_t *);

Q_ENDCXX /* */
#endif // ifndef QT_SINC_BARRIER_H
/* vim:set expandtab: */
