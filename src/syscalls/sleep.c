#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h> /* for uint64_t */

#ifdef HAVE_SYS_SYSCALL_H
# include <unistd.h>
# include <sys/syscall.h>        /* for SYS_accept and others */
#endif

/* API Headers */
#include "qthread/qthread.h"
#include "qthread/qtimer.h"

/* Internal Headers */
#include "qt_io.h"
#include "qt_asserts.h"
#include "qthread_innards.h" /* for qlib */
#include "qt_qthread_mgmt.h"

unsigned int sleep(unsigned int seconds)
{
  printf("entering sleep\n");
    if (qt_blockable()) {
        qtimer_t t = qtimer_create();
        qtimer_start(t);
        do {
            qthread_yield();
            qtimer_stop(t);
        } while (qtimer_secs(t) < seconds);
        qtimer_destroy(t);
        return 0;
    } else {
#if HAVE_SYSCALL
# if HAVE_DECL_SYS_SLEEP
        {
            struct timespec tv;
            tv.tv_sec = seconds;
            tv.tv_usec = 0;
            return syscall(SYS_sleep, &tv);
        }
# elif HAVE_DECL_SYS_USLEEP
        {
            struct timespec tv;
            tv.tv_sec = seconds;
            tv.tv_usec = 0;
            return syscall(SYS_usleep, &tv);
        }
# elif HAVE_DECL_SYS_NANOSLEEP
        {
            struct timespec tv, rem;
            tv.tv_sec = seconds;
            tv.tv_nsec = 0;
            return syscall(SYS_nanosleep, &tv, &rem);
        }

# else
        return 0;
# endif
#else   /* if HAVE_SYSCALL */
        return 0;
#endif  /* if HAVE_SYSCALL */
    }
}

/* vim:set expandtab: */
