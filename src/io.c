#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h>       /* for uint64_t */
#include <stdio.h>                     /* for fprintf() */
/* - syscall(2) */
#include <sys/syscall.h>
#include <unistd.h>
/* - accept(2) */
#include <sys/socket.h>
/* - connect(2) */
#include <sys/types.h>
#include <sys/socket.h>
/* - nanosleep(2) */
#include <time.h>
/* - poll(2) */
#include <poll.h>
/* - read(2) */
#include <sys/uio.h>
/* - select(2) */
#include <sys/select.h>

/* Internal Headers */
#include "qt_io.h"
#include "qthread_asserts.h"
#include "qthread_innards.h"
#include "qt_threadqueues.h"

typedef struct {
    qt_blocking_queue_node_t *head;
    qt_blocking_queue_node_t *tail;
    pthread_mutex_t           lock;
    pthread_cond_t            notempty;
} qt_blocking_queue_t;

static qt_blocking_queue_t theQueue;
qt_mpool                   syscall_job_pool = NULL;
static pthread_t           proxy_thread;
static volatile int        proxy_exit = 0;

static void qt_blocking_subsystem_internal_teardown(void)
{
    proxy_exit = 1;
    pthread_join(proxy_thread, NULL);
    qt_mpool_destroy(syscall_job_pool);
    QTHREAD_DESTROYLOCK(&theQueue.lock);
    QTHREAD_DESTROYCOND(&theQueue.notempty);
}

static void *qt_blocking_subsystem_proxy_thread(void *arg)
{
    while (proxy_exit == 0) {
        qt_process_blocking_calls();
    }
    return 0;
}

void qt_blocking_subsystem_init(void)
{
    syscall_job_pool = qt_mpool_create(sizeof(qt_blocking_queue_node_t));
    theQueue.head    = NULL;
    theQueue.tail    = NULL;
    qassert(pthread_mutex_init(&theQueue.lock, NULL), 0);
    qassert(pthread_cond_init(&theQueue.notempty, NULL), 0);
    {
        int r;
        if ((r = pthread_create(&proxy_thread, NULL, qt_blocking_subsystem_proxy_thread, NULL)) != 0) {
            fprintf(stderr, "qt_blocking_subsystem_init: pthread_create() failed (%d)\n", r);
            perror("qt_blocking_subsystem_init spawning proxy thread");
            abort();
        }
    }
    qthread_internal_cleanup(qt_blocking_subsystem_internal_teardown);
}

void qt_process_blocking_calls(void)
{
    qt_blocking_queue_node_t *item;

    QTHREAD_LOCK(&theQueue.lock);
    while (theQueue.head == NULL) {
        struct timeval  tv;
        struct timespec ts;
        int             ret;
        gettimeofday(&tv, NULL);
        ts.tv_sec  = tv.tv_sec + 1;
        ts.tv_nsec = 0;
        ret        = pthread_cond_timedwait(&theQueue.notempty, &theQueue.lock, &ts);
        if (ret == ETIMEDOUT) {
            QTHREAD_UNLOCK(&theQueue.lock);
            return;
        }
        assert(ret == 0);
    }
    item = theQueue.head;
    assert(item != NULL);
    theQueue.head = item->next;
    QTHREAD_UNLOCK(&theQueue.lock);
    item->next = NULL;
    /* do something with <item> */
    switch(item->op) {
        case ACCEPT:
        {
            int socket;
            memcpy(&socket, &item->args[0], sizeof(int));
            item->ret = syscall(SYS_accept,
                                socket,
                                (struct sockaddr *)item->args[1],
                                (socklen_t *)item->args[2]);
            break;
        }
        case CONNECT:
        {
            int socket;
            memcpy(&socket, &item->args[0], sizeof(int));
            item->ret = syscall(SYS_connect,
                                socket,
                                (const struct sockaddr *)item->args[1],
                                (socklen_t)item->args[2]);
            break;
        }
        default:
            fprintf(stderr, "Unhandled syscall: %i\n", item->op);
            abort();
        case POLL:
            item->ret = syscall(SYS_poll,
                                (struct pollfd *)item->args[0],
                                (nfds_t)item->args[1],
                                (int)item->args[2]);
            break;
        case READ:
        {
            int fd;
            memcpy(&fd, &item->args[0], sizeof(int));
            item->ret = syscall(SYS_read,
                                fd,
                                (void *)item->args[1],
                                (size_t)item->args[2]);
            break;
        }
        /* case RECV:
         * case RECVFROM: */
        case SELECT:
            item->ret = syscall(SYS_select,
                                (int)item->args[0],
                                (fd_set *)item->args[1],
                                (fd_set *)item->args[2],
                                (fd_set *)item->args[3],
                                (struct timeval *)item->args[4]);
            break;
            /* case SEND:
             * case SENDTO: */
            /* case SIGWAIT: */
#if HAVE_DECL_SYS_SYSTEM
        case SYSTEM:
            item->ret = syscall(SYS_system,
                                (const char *)item->args[0]);
            break;
#endif
        case WAIT4:
            item->ret = syscall(SYS_wait4,
                                (pid_t)item->args[0],
                                (int *)item->args[1],
                                (int)item->args[2],
                                (struct rusage *)item->args[3]);
            break;
        case WRITE:
            item->ret = syscall(SYS_write,
                                (int)item->args[0],
                                (const void *)item->args[1],
                                (size_t)item->args[2],
                                (off_t)item->args[3]);
    }
    /* and now, re-queue */
    qt_threadqueue_enqueue(item->thread->rdata->shepherd_ptr->ready, item->thread, item->thread->rdata->shepherd_ptr);
}

void qt_blocking_subsystem_enqueue(qt_blocking_queue_node_t *job)
{
    qt_blocking_queue_node_t *prev;

    QTHREAD_LOCK(&theQueue.lock);
    prev          = theQueue.tail;
    theQueue.tail = job;
    if (prev == NULL) {
        theQueue.head = job;
    } else {
        prev->next = job;
    }
    QTHREAD_SIGNAL(&theQueue.notempty);
    QTHREAD_UNLOCK(&theQueue.lock);
}

/* vim:set expandtab: */
