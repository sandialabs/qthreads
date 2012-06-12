#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "qthread/qthread.h"

#include "qthread/multinode.h"
#include "qt_multinode_innards.h"
#include "qthread/spr.h"

#include "qthread_innards.h"
#include "qt_debug.h"
#include "qt_atomics.h"
#include "net/net.h"

static int       initialized_flags = 0;

int spr_init(unsigned int flags,
             qthread_f   *regs)
{
    qassert(setenv("QT_MULTINODE", "1", 1), 0);
    if (flags & ~(SPR_SPMD)) return SPR_BADARGS;
    initialized_flags = flags;
    qthread_initialize();
    if (regs) {
        qthread_f *cur_f = regs;
        uint32_t   tag   = 1;
        while (*cur_f) {
            qassert(qthread_multinode_register(tag + 1000, *cur_f), QTHREAD_SUCCESS);
            cur_f = regs + tag;
            ++tag;
        }
    }
    if (flags & SPR_SPMD) {
        qthread_multinode_multistart();
    } else {
        qthread_multinode_run();
    }
    return SPR_OK;
}

int spr_fini(void)
{
    if (initialized_flags & SPR_SPMD) {
        qthread_multinode_multistop();
    }

    return SPR_OK;
}

int spr_num_locales(void)
{
    return qthread_multinode_size();
}

int spr_locale_id(void)
{
    return qthread_multinode_rank();
}

