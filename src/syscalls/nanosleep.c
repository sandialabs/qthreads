#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* System Headers */
#include <stdint.h>
#include <time.h>

#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h> /* for SYS_accept and others */
#include <unistd.h>
#endif

/* API Headers */
#include "qthread/qthread.h"
#include "qthread/qtimer.h"

/* Internal Headers */
#include "qt_io.h"
#include "qt_qthread_mgmt.h"
#include "qthread_innards.h" /* for qlib */

#if HAVE_SYSCALL && HAVE_DECL_SYS_NANOSLEEP
static int qt_nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
  if (qt_blockable()) {
    qtimer_t t = qtimer_create();
    double seconds = rqtp->tv_sec + (rqtp->tv_nsec * 1e-9);

    qtimer_start(t);
    do {
      qthread_yield();
      qtimer_stop(t);
    } while (qtimer_secs(t) < seconds);
    if (rmtp) {
      time_t secs = (time_t)qtimer_secs(t);
      rmtp->tv_sec = rqtp->tv_sec - secs;
      rmtp->tv_nsec = (long)(qtimer_secs(t) - (double)secs) * 1e9;
    }
    qtimer_destroy(t);
    return 0;
  } else {
    if (rmtp) { *rmtp = *rqtp; }
    return -1;
  }
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
  if (qt_blockable()) {
    return qt_nanosleep(rqtp, rmtp);
  } else {
    return syscall(SYS_nanosleep, rqtp, rmtp);
  }
}

#endif /* if HAVE_SYSCALL && HAVE_DECL_SYS_NANOSLEEP */

/* vim:set expandtab: */
