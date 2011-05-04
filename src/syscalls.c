#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h>       /* for uint64_t */

/* Internal Headers */
#include "qt_io.h"
#include "qthread_asserts.h"
#include "qthread/qthread.h"
#include "qthread/qtimer.h"

unsigned int sleep(unsigned int seconds)
{
    if ((qlib != NULL) && (qthread_internal_self() != NULL)) {
        qtimer_t t = qtimer_create();
        qtimer_start(t);
        do {
            qthread_yield();
            qtimer_stop(t);
        } while (qtimer_secs(t) < seconds);
        return 0;
    } else {
#if HAVE_DECL_SYS_SLEEP
        return syscall(SYS_sleep, seconds);

#elif HAVE_DECL_SYS_USLEEP
        return syscall(SYS_usleep, seconds * 1e6);

#elif HAVE_DECL_SYS_NANOSLEEP
        return syscall(SYS_nanosleep, seconds * 1e9);

#else
        return 0;
#endif
    }
}

int usleep(useconds_t useconds)
{
    if ((qlib != NULL) && (qthread_internal_self() != NULL)) {
        qtimer_t t       = qtimer_create();
        double   seconds = useconds * 1e-6;
        qtimer_start(t);
        do {
            qthread_yield();
            qtimer_stop(t);
        } while (qtimer_secs(t) < seconds);
        return 0;
    } else {
#if HAVE_DECL_SYS_USLEEP
        return syscall(SYS_usleep, useconds);

#elif HAVE_DECL_SYS_NANOSLEEP
        return syscall(SYS_nanosleep, useconds * 1e3);

#else
        return 0;
#endif
    }
}

int nanosleep(const struct timespec *rqtp,
              struct timespec       *rmtp)
{
    if ((qlib != NULL) && (qthread_internal_self() != NULL)) {
        qtimer_t t       = qtimer_create();
        double   seconds = rqtp->tv_sec + (rqtp->tv_nsec * 1e-9);

        qtimer_start(t);
        do {
            qthread_yield();
            qtimer_stop(t);
        } while (qtimer_secs(t) < seconds);
        return 0;
    } else {
#if HAVE_DECL_SYS_NANOSLEEP
        return syscall(SYS_nanosleep, rqtp, rmtp);

#else
        return 0;
#endif
    }
}

/* vim:set expandtab: */
