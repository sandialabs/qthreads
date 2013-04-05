#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qthread/qthread.h"
#include "qt_asserts.h"
#include "qt_debug.h"
#include "qt_queue.h"
#include "qt_visibility.h"

qthread_queue_t API_FUNC qthread_queue_create(uint8_t flags,
                                              size_t  length)
{
    qthread_queue_t q = calloc(1, sizeof(struct qthread_queue_s));

    assert(q);
    if (flags & QTHREAD_QUEUE_MULTI_JOIN) {
        q->type = NEMESIS;
    } else if (flags & QTHREAD_QUEUE_MULTI_JOIN_LENGTH) {
        q->type = NEMESIS_LENGTH;
    } else if (flags & QTHREAD_QUEUE_CAPPED) {
        q->type                 = CAPPED;
        q->q.capped.maxmembers  = (aligned_t)length;
        q->q.capped.membercount = 0;
        q->q.capped.members     = MALLOC(sizeof(qthread_t *) * length);
        assert(q->q.capped.members);
    } else {
        q->type = NONE;
    }
    return q;
}

int API_FUNC qthread_queue_join(qthread_queue_t q)
{
    assert(q);
    switch(q->type) {
        case NEMESIS:
        case NEMESIS_LENGTH:
            break;
        case CAPPED:
            break;
    }
}

int API_FUNC qthread_queue_release_one(qthread_queue_t q)
{
    assert(q);
    switch(q->type) {
        case NEMESIS:
        case NEMESIS_LENGTH:
            break;
        case CAPPED:
            break;
    }
}

int API_FUNC qthread_queue_release_all(qthread_queue_t q)
{
    assert(q);
    switch(q->type) {
        case NEMESIS:
            break;
        case NEMESIS_LENGTH:
            break;
        case CAPPED:
            break;
    }
}

int API_FUNC qthread_queue_destroy(qthread_queue_t q)
{
    assert(q);
    switch(q->type) {
        case NEMESIS:
        case NEMESIS_LENGTH:
            break;
        case CAPPED:
            FREE(q->q.capped.members, sizeof(qthread_t *) * q->q.capped.maxmembers);
            break;
    }
    FREE(q, sizeof(struct qthread_queue_s));
    return QTHREAD_SUCCESS;
}

/* vim:set expandtab: */
