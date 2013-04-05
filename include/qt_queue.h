#ifndef QT_QUEUE_H
#define QT_QUEUE_H

#include "qt_qthread_t.h"

/* This queue uses the NEMESIS lock-free queue protocol from
 * http://www.mcs.anl.gov/~buntinas/papers/ccgrid06-nemesis.pdf
 */

struct qthread_queue_node_s {
    struct qthread_queue_node_s *next;
    qthread_t                   *thread;
};

typedef struct qthread_queue_NEMESIS_s {
    /* The First Cacheline */
    void   *head;
    void   *tail;
    uint8_t pad1[CACHELINE_WIDTH - (2 * sizeof(void *))];
    /* The Second Cacheline */
    void   *shadow_head;
    aligned_t length;
    uint8_t pad2[CACHELINE_WIDTH - sizeof(void *) - sizeof(aligned_t)];
} qthread_queue_NEMESIS_t;

typedef struct qthread_queue_none_s {
    void *head;
    void *tail;
} qthread_queue_none_t;

typedef struct qthread_queue_capped_s {
    qthread_t **members;
    aligned_t membercount;
    aligned_t maxmembers;
} qthread_queue_capped_t;

enum qthread_queue_synctype {
    NONE,
    NEMESIS, /* multi-join, emptying is user's synch */
    MTS, /* multi-join, multi-empty (UNIMPLEMENTED) */
    NEMESIS_LENGTH, /* multi-join, w/ atomic length */
    CAPPED
};

struct qthread_queue_s {
    enum qthread_queue_synctype type;
    union {
        qthread_queue_NEMESIS_t nemesis;
        qthread_queue_none_t none;
        qthread_queue_capped_t capped;
    } q;
};

#endif // ifndef QT_QUEUE_H
/* vim:set expandtab: */
