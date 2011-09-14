#ifndef FIFTY_SIX_RWLOCK_H
#define FIFTY_SIX_RWLOCK_H

/**
 * This reader-writer lock is derived from
 * "TLRW: Return of the Read-Write Lock"
 * Dice and Shavit, SPAA '10
 *
 * It supports at most 56 readers and an
 * unlimited number of writers. If the lock is used
 * with more than 56 readers, then unpredictable
 * behavior will happen.
 **/

#include <stdint.h>

#if (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32) || \
    (QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64)
# define cpu_relax() do { __asm__ __volatile__ ("pause" ::: "memory"); } while (0)
#else
# define cpu_relax() do { } while (0)
#endif

struct tlrw_lock {
    unsigned int owner;
    uint8_t  readers[64 - sizeof(unsigned int) * 2] __attribute__((aligned(8)));
};

typedef struct tlrw_lock rwlock_t;

static QINLINE void rwlock_init(rwlock_t *l) {
    unsigned int i;

    l->owner = 0;
    for (i = 0; i < sizeof l->readers; i++)
        l->readers[i] = 0;

    return;
}

static QINLINE void rwlock_rdlock(rwlock_t *l) {
    int id = qthread_worker(NULL);

    for (;;) {
        l->readers[id] = 1;
        COMPILER_FENCE; 

        if (l->owner == 0) {
            break;
        } else {
            l->readers[id] = 0;

            while (l->owner != 0)
                cpu_relax();
        }
    }

    return;
}

static QINLINE void rwlock_rdunlock(rwlock_t *l) {
    int id = qthread_worker(NULL);
    l->readers[id] = 0;
}

static QINLINE void rwlock_wrlock(rwlock_t *l) {
    int id = qthread_worker(NULL) + 1;
    uint64_t *readers = (void *) l->readers;
    while(qthread_cas32(&(l->owner), 0, id) != id)
        cpu_relax();

    COMPILER_FENCE; 
    for (int i = 0; i < sizeof(l->readers) / 8; i++) {
        while (readers[i] != 0) 
            cpu_relax();
    }

    return;
}

static QINLINE void rwlock_wrunlock(rwlock_t *l) {
    l->owner = 0;
    COMPILER_FENCE; 
}

#endif // ifndef FIFTY_SIX_RWLOCK_H
/* vim:set expandtab: */
