#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h>       /* for uint64_t */
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

static void qt_blocking_subsystem_internal_teardown(void)
{
    QTHREAD_DESTROYLOCK(&theQueue.lock);
    QTHREAD_DESTROYCOND(&theQueue.notempty);
}

void qt_blocking_subsystem_init(void)
{
    theQueue.head = NULL;
    theQueue.tail = NULL;
    qassert(pthread_mutex_init(&theQueue.lock, NULL), 0);
    qassert(pthread_cond_init(&theQueue.notempty, NULL), 0);
    qthread_internal_cleanup(qt_blocking_subsystem_internal_teardown);
}

void qt_process_blocking_calls(void)
{
    qt_blocking_queue_node_t *item;

    QTHREAD_LOCK(&theQueue.lock);
    while (theQueue.head == NULL) {
        QTHREAD_CONDWAIT(&theQueue.notempty, &theQueue.lock);
    }
    item = theQueue.head;
    assert(item != NULL);
    theQueue.head = item->next;
    QTHREAD_UNLOCK(&theQueue.lock);
    item->next = NULL;
    /* do something with <item> */
    switch(item->op) {
        case ACCEPT:
            item->ret = syscall(SYS_accept,
                                (int)item->args[0],
                                (struct sockaddr *)item->args[1],
                                (socklen_t *)item->args[2]);
            break;
        case CONNECT:
            item->ret = syscall(SYS_connect,
                                (int)item->args[0],
                                (const struct sockaddr *)item->args[1],
                                (socklen_t)item->args[2]);
            break;
        default:
            fprintf(stderr, "Unhandled syscall: %i\n", item->op);
            abort();
        case FORK:
            fprintf(stderr, "What the heck does fork() mean in this context?\n");
            abort();
#ifdef HAVE_SYS_NANOSLEEP
        case NANOSLEEP:
            item->ret = syscall(SYS_nanosleep,
                                (const struct timespec *)item->args[0],
                                (struct timespec *)item->args[1]);
            break;
#endif
        case POLL:
            item->ret = syscall(SYS_poll,
                                (struct pollfd *)item->args[0],
                                (nfds_t)item->args[1],
                                (int)item->args[2]);
            break;
        case READ:
            item->ret = syscall(SYS_read,
                                (int)item->args[0],
                                (void *)item->args[1],
                                (size_t)item->args[2]);
            break;
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
#ifdef HAVE_SYS_SLEEP
        case SLEEP:
            item->ret = syscall(SYS_sleep,
                                (unsigned int)item->args[0]); // XXX: item->ret should be unsigned
            break;
#endif
#ifdef HAVE_SYS_SYSTEM
        case SYSTEM:
            item->ret = syscall(SYS_system,
                                (const char *)item->args[0]);
            break;
#endif
#ifdef HAVE_SYS_USLEEP
        case USLEEP:
            item->ret = syscall(SYS_usleep,
                                (useconds_t)item->args[0]);
            break;
#endif
        case WAIT4:
            item->ret = syscall(SYS_wait4,
                                (pid_t)item->args[0],
                                (int *)item->args[1],
                                (int)item->args[2],
                                (struct rusage*)item->args[3]);
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
