#ifndef QTHREAD_QDQUEUE_H
#define QTHREAD_QDQUEUE_H

#include <qthread/qthread.h>

Q_STARTCXX			       /* */
typedef struct qdqueue_s qdqueue_t;

/* Create a new qdqueue */
qdqueue_t *qdqueue_create(void);

/* destroy that queue */
int qdqueue_destroy(qthread_t * me, qdqueue_t * q);

/* enqueue something in the queue */
int qdqueue_enqueue(qthread_t * me, qdqueue_t * q, void *elem);

/* enqueue something in the queue at a given location */
int qdqueue_enqueue_there(qthread_t * me, qdqueue_t * q, void *elem,
			  qthread_shepherd_id_t there);

/* dequeue something from the queue (returns NULL for an empty queue) */
void *qdqueue_dequeue(qthread_t * me, qdqueue_t * q);

/* returns 1 if the queue is empty, 0 otherwise */
int qdqueue_empty(qthread_t * me, qdqueue_t * q);

Q_ENDCXX			       /* */
#endif
