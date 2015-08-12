#ifndef _QT_YIELD_H_
#define _QT_YIELD_H_

/* Flags for uber-yield (32-bit unsigned int) */
#define QTHREAD_YIELD_NEAR        (1 << 0)
#define QTHREAD_YIELD_FAR         (1 << 1)
#define QTHREAD_YIELD_DIRECT      (1 << 2)
#define QTHREAD_YIELD_STEALABLE   (1 << 3)
#define QTHREAD_YIELD_UNSTEALABLE (1 << 4)

#endif
