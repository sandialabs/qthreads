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
    qtimer_t t = qtimer_create();
    qtimer_start(t);
    do {
	qthread_yield();
	qtimer_stop(t);
    } while (qtimer_secs(t) < seconds);
    return 0;
}

int usleep(useconds_t useconds)
{
    qtimer_t t = qtimer_create();
    double seconds = useconds * 1e-6;
    qtimer_start(t);
    do {
	qthread_yield();
	qtimer_stop(t);
    } while (qtimer_secs(t) < seconds);
    return 0;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    qtimer_t t = qtimer_create();
    double seconds = rqtp->tv_sec + (rqtp->tv_nsec * 1e-9);
    qtimer_start(t);
    do {
	qthread_yield();
	qtimer_stop(t);
    } while (qtimer_secs(t) < seconds);
    return 0;
}
