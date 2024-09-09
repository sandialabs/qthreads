#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

/* System Headers */
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h> /* for SYS_accept and others */
#include <unistd.h>
#endif

/* API Headers */
#include "qthread/qthread.h"
#include "qthread/qtimer.h"

/* Internal Headers */
#include "qt_asserts.h"
#include "qt_io.h"
#include "qt_qthread_mgmt.h"
#include "qthread_innards.h" /* for qlib */

#if HAVE_SYSCALL && HAVE_DECL_SYS_SLEEP
static unsigned int qt_sleep(unsigned int seconds) {
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
    return seconds;
  }
}

unsigned int sleep(unsigned int seconds) {
  if (qt_blockable()) {
    return qt_sleep(seconds);
  } else {
    return syscall(SYS_sleep, seconds);
  }
}

#endif /* if HAVE_SYSCALL && HAVE_DECL_SYS_SLEEP */

/* vim:set expandtab: */
