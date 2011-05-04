#ifndef QT_IO_H
#define QT_IO_H

#include "qt_qthread_struct.h"

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
    WAIT4,
    WRITE
} syscall_t;

typedef struct _qt_blocking_queue_node_s {
    struct _qt_blocking_queue_node_s *next;
    qthread_t                        *thread;
    syscall_t                         op;
    uintptr_t                         args[5];
    int                               ret;
} qt_blocking_queue_node_t;

extern qt_mpool syscall_job_pool;

void qt_blocking_subsystem_init(void);
void qt_process_blocking_calls(void);
void qt_blocking_subsystem_enqueue(qt_blocking_queue_node_t *job);

#endif // ifndef QT_IO_H
/* vim:set expandtab: */
