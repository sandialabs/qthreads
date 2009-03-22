#ifndef QTHREAD_QLFQUEUE_H
#define QTHREAD_QLFQUEUE_H

typedef struct qlfqueue_s qlfqueue_t;

/* Create a new qlfqueue */
qlfqueue_t *qlfqueue_new(qthread_t *me);

/* destroy that queue */
int qlfqueue_destroy(qthread_t *me, qlfqueue_t * q);

/* enqueue something in the queue */
int qlfqueue_enqueue(qthread_t *me, qlfqueue_t * q, void *elem);

/* dequeue something from the queue (returns NULL for an empty queue) */
void *qlfqueue_dequeue(qthread_t *me, qlfqueue_t * q);

/* returns 1 if the queue is empty, 0 otherwise */
int qlfqueue_empty(qlfqueue_t *q);


#endif
