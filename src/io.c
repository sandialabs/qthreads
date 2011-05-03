#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <qthread/qthread-int.h>       /* for uint64_t */

/* Internal Headers */
#include "qt_io.h"

typedef enum blocking_syscalls {
    ACCEPT,
    CONNECT,
    FORK,
    NANOSLEEP,
    POLL,
    READ,
    /*RECV,
     * RECVFROM,*/
    SELECT,
    /*SEND,
     * SENDTO,*/
    /*SIGWAIT,*/
    SLEEP,
    SYSTEM,
    USLEEP,
    WAITPID,
    WRITE
} syscall_t;

typedef struct _qt_blocking_queue_s {
    struct _qt_blocking_queue_node_s *next;
    qthread_t                        *thread;
    syscall_t                         op;
    uintptr_t                         args[5];
} qt_blocking_queue_node_t;

typedef struct {
    qt_blocking_queue_node_t *head;
    qt_blocking_queue_node_t *tail;
    pthread_mutex_t           lock;
    pthread_cond_t            notempty;
} qt_blocking_queue_t;

static qt_blocking_queue_t theQueue;

static void qt_blocking_subsystem_internal_teardown(void)
{
    QTHREAD_DESTROYLOCK(&theQueue->lock);
    QTHREAD_DESTROYCOND(&theQueu->notempty);
}

int qt_blocking_subsystem_init(void)
{
    theQueue->head = NULL;
    theQueue->tail = NULL;
    QTHREAD_INITLOCK(&theQueue->lock);
    qassert(pthread_cond_init(&theQueue->notempty, NULL), 0);
    qthread_internal_cleanup(qt_blocking_subsystem_internal_teardown);
    return QTHREAD_SUCCESS;
}

void qt_process_blocking_calls(void)
{
    qt_blocking_queue_node_t *item;

    QTHREAD_LOCK(&theQueue->lock);
    while (theQueue->head == NULL) {
        QTHREAD_CONDWAIT(&theQueue->notempty, &theQueue->lock);
    }
    item = theQueue->head;
    assert(item != NULL);
    theQueue->head = item->next;
    QTHREAD_UNLOCK(&theQueue->lock);
    item->next = NULL;
    /* do something with <item> */
}

void qt_blocking_subsystem_enqueue(qthread_t *t)
{
    qt_blocking_queue_node_t *job = malloc(sizeof(qt_blocking_queue_node_t));
    qt_blocking_queue_node_t *prev;

    job->next   = NULL;
    job->thread = t;
    QTHREAD_LOCK(&theQueue->lock);
    prev           = theQueue->tail;
    theQueue->tail = job;
    if (prev == NULL) {
        theQueue->head = job;
    } else {
        prev->next = job;
    }
    QTHREAD_SIGNAL(&theQueue->notempty);
    QTHREAD_UNLOCK(&theQueue->lock);
}

/* vim:set expandtab: */
