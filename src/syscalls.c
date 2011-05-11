#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h> /* for uint64_t */
#include <sys/syscall.h>         /* for SYS_accept and others */

/* Internal Headers */
#include "qt_io.h"
#include "qthread_asserts.h"
#include "qthread_innards.h" /* for qlib */
#include "qthread/qthread.h"
#include "qthread/qtimer.h"

typedef QT_SOCKLENTYPE_T socklen_t;
struct sockaddr {};

unsigned int sleep(unsigned int seconds)
{   /*{{{*/
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
} /*}}}*/

int usleep(useconds_t useconds)
{   /*{{{*/
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
} /*}}}*/

int nanosleep(const struct timespec *rqtp,
              struct timespec       *rmtp)
{   /*{{{*/
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
} /*}}}*/

int accept(int                       socket,
           struct sockaddr *restrict address,
           socklen_t *restrict       address_len)
{
    qthread_t *me;

    if ((qlib != NULL) && ((me = qthread_internal_self()) != NULL)) {
        qt_blocking_queue_node_t *job = qt_mpool_alloc(syscall_job_pool);
        int                       ret;

        job->thread = me;
        job->op     = ACCEPT;
        memcpy(&job->args[0], &socket, sizeof(int));
        job->args[1] = (uintptr_t)address;
        job->args[2] = (uintptr_t)address_len;

        me->rdata->blockedon = (struct qthread_lock_s *)job;
        me->thread_state     = QTHREAD_STATE_SYSCALL;
        qthread_back_to_master(me);
        ret = job->ret;
        qt_mpool_free(syscall_job_pool, job);
        return ret;
    } else {
        return syscall(SYS_accept, socket, address, address_len);
    }
}

int connect(int                       socket,
            struct sockaddr *restrict address,
            socklen_t *restrict       address_len)
{
    qthread_t *me;

    if ((qlib != NULL) && ((me = qthread_internal_self()) != NULL)) {
        qt_blocking_queue_node_t *job = qt_mpool_alloc(syscall_job_pool);
        int                       ret;

        job->thread = me;
        job->op     = CONNECT;
        memcpy(&job->args[0], &socket, sizeof(int));
        job->args[1] = (uintptr_t)address;
        job->args[2] = (uintptr_t)address_len;

        me->rdata->blockedon = (struct qthread_lock_s *)job;
        qthread_back_to_master(me);
        ret = job->ret;
        qt_mpool_free(syscall_job_pool, job);
        return ret;
    } else {
        return syscall(SYS_accept, socket, address, address_len);
    }
}

ssize_t read(int    filedes,
             void  *buf,
             size_t nbyte)
{
    qthread_t *me;

    if ((qlib != NULL) && ((me = qthread_internal_self()) != NULL)) {
        qt_blocking_queue_node_t *job = qt_mpool_alloc(syscall_job_pool);
        int                       ret;

        assert(job);
        job->thread = me;
        job->op     = READ;
        memcpy(&job->args[0], &filedes, sizeof(int));
        job->args[1] = (uintptr_t)buf;
        job->args[2] = (uintptr_t)nbyte;

        assert(me->rdata);

        me->rdata->blockedon = (struct qthread_lock_s *)job;
        me->thread_state     = QTHREAD_STATE_SYSCALL;
        qthread_back_to_master(me);
        ret = job->ret;
        qt_mpool_free(syscall_job_pool, job);
        return ret;
    } else {
        return syscall(SYS_read, filedes, buf, nbyte);
    }
}

/* vim:set expandtab: */
