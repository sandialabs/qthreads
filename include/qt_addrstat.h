#ifndef QT_ADDRSTAT_H
#define QT_ADDRSTAT_H

#include "qt_blocking_structs.h"

/* This allocates a new, initialized addrstat structure, which is used for
 * keeping track of the FEB status of an address. It expects a shepherd pointer
 * to use to find the right memory pool to use. */
static QINLINE qthread_addrstat_t *qthread_addrstat_new(qthread_shepherd_t *shepherd)
{                                      /*{{{ */
    qthread_addrstat_t *ret = ALLOC_ADDRSTAT(shepherd);

    if (ret != NULL) {
        QTHREAD_FASTLOCK_INIT(ret->lock);
        ret->full  = 1;
        ret->valid = 1;
        ret->EFQ   = NULL;
        ret->FEQ   = NULL;
        ret->FFQ   = NULL;
        QTHREAD_EMPTY_TIMER_INIT(ret);
    }
    return ret;
}                                      /*}}} */

/* this function is for maintenance of the FEB hashtables. SHOULD only be
 * necessary for things left over when qthread_finalize is called */
static void qthread_addrstat_delete(qthread_addrstat_t *m)
{                                      /*{{{ */
#ifdef QTHREAD_LOCK_PROFILING
    qtimer_destroy(m->empty_timer);
#endif
    QTHREAD_FASTLOCK_DESTROY(m->lock);
    FREE_ADDRSTAT(m);
}                                      /*}}} */

static void qt_hash_print_addrstat(const qt_key_t addr, qthread_addrstat_t *m, void *arg)
{                                      /*{{{ */
    printf("addr: %#lx\n", (unsigned long)addr);
    QTHREAD_FASTLOCK_LOCK(&m->lock);
    printf("\tEFQ = %p\n"
           "\tFEQ = %p\n"
           "\tFFQ = %p\n"
           "\tfull = %u\n"
           "\tvalid = %u\n",
           m->EFQ, m->FEQ, m->FFQ, m->full, m->valid);
    *(int*)arg += 1;
    QTHREAD_FASTLOCK_UNLOCK(&m->lock);
}                                      /*}}} */

#endif // ifndef QT_ADDRSTAT_H
/* vim:set expandtab: */
