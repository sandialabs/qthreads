// This lock-free algorithm borrowed from
// http://www.research.ibm.com/people/m/michael/podc-1996.pdf

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>		       /* for malloc() */
#include <qthread/qthread.h>
#include <qthread/qlfqueue.h>

#include <qthread/qpool.h>
#include "qthread_asserts.h"
#include "qt_atomics.h"		       /* for qt_cas() */

/* queue declarations */
typedef struct _qlfqueue_node
{
    void *value;
    volatile struct _qlfqueue_node *next;
} qlfqueue_node_t;

struct qlfqueue_s		/* typedef'd to qlfqueue_t */
{
    volatile qlfqueue_node_t *head;
    volatile qlfqueue_node_t *tail;
};

static qpool qlfqueue_node_pool = NULL;

/* to avoid ABA reinsertion trouble, each pointer in the queue needs to have a
 * monotonically increasing counter associated with it. The counter doesn't
 * need to be huge, just big enough to avoid trouble. We'll
 * just claim 4, to be conservative. Thus, a qlfqueue_node_t must be aligned to 16 bytes. */
#define QCTR_MASK (15)
#define QPTR(x) ((qlfqueue_node_t*)(((uintptr_t)(x))&~(uintptr_t)QCTR_MASK))
#define QCTR(x) ((unsigned char)(((uintptr_t)(x))&QCTR_MASK))
#define QCOMPOSE(x,y) (void*)(((uintptr_t)QPTR(x))|((QCTR(y)+1)&QCTR_MASK))

qlfqueue_t *qlfqueue_new(qthread_t *me)
{				       /*{{{ */
    qlfqueue_t *q;

    if (qlfqueue_node_pool == NULL) {
	qlfqueue_node_pool =
	    qpool_create_aligned(me, sizeof(qlfqueue_node_t), 16);
    }
    assert(qlfqueue_node_pool != NULL);

    q = malloc(sizeof(struct qlfqueue_s));
    if (q != NULL) {
	q->head = (qlfqueue_node_t *) qpool_alloc(me, qlfqueue_node_pool);
	assert(q->head != NULL);
	if (QPTR(q->head) == NULL) {   // if we're not using asserts, fail nicely
	    free(q);
	    q = NULL;
	}
	q->tail = q->head;
	QPTR(q->tail)->next = NULL;
    }
    return q;
}				       /*}}} */

int qlfqueue_destroy(qthread_t *me, qlfqueue_t * q)
{				       /*{{{ */
    qargnonull(q);
    while (QPTR(q->head) != QPTR(q->tail)) {
	qlfqueue_dequeue(me, q);
    }
    qpool_free(me, qlfqueue_node_pool, QPTR(q->head));
    free(q);
    return QTHREAD_SUCCESS;
}				       /*}}} */

int qlfqueue_enqueue(qthread_t *me, qlfqueue_t * q, void *elem)
{				       /*{{{ */
    qlfqueue_node_t *tail;
    qlfqueue_node_t *node, *next;

    qargnonull(elem);
    qargnonull(q);
    qargnonull(me);

    node = (qlfqueue_node_t *) qpool_alloc(me, qlfqueue_node_pool);
    // these asserts should be redundant
    assert(node != NULL);
    assert((((uintptr_t) node) & QCTR_MASK) == 0);	// node MUST be aligned

    node->value = elem;
    // set to null without disturbing the ctr
    node->next = (qlfqueue_node_t *) (uintptr_t) QCTR(node->next);

    while (1) {
	tail = (qlfqueue_node_t *) (q->tail);
	next = (qlfqueue_node_t *) (QPTR(tail)->next);
	if (tail == q->tail) {	       // are tail and next consistent?
	    if (QPTR(next) == NULL) {  // was tail pointing to the last node?
		if (qt_cas
		    ((volatile void **)&(QPTR(tail)->next), next,
		     QCOMPOSE(node, next)) == next)
		    break;	       // success!
	    } else {		       // tail not pointing to last node
		(void)qt_cas((volatile void **)&(q->tail), tail,
			     QCOMPOSE(next, tail));
	    }
	}
    }
    (void)qt_cas((volatile void **)&(q->tail), tail, QCOMPOSE(node, tail));
    return QTHREAD_SUCCESS;
}				       /*}}} */

void *qlfqueue_dequeue(qthread_t *me, qlfqueue_t * q)
{				       /*{{{ */
    void *p = NULL;
    qlfqueue_node_t *head, *tail, *next;

    assert(q != NULL);
    if (q == NULL) {
	return NULL;
    }
    while (1) {
	head = (qlfqueue_node_t *) (q->head);
	tail = (qlfqueue_node_t *) (q->tail);
	next = (qlfqueue_node_t *) (QPTR(head)->next);
	if (head == q->head) {	       // are head, tail, and next consistent?
	    if (QPTR(head) == QPTR(tail)) {	// is queue empty or tail falling behind?
		if (QPTR(next) == NULL) {	// is queue empty?
		    return NULL;
		}
		(void)qt_cas((volatile void **)&(q->tail), tail, QCOMPOSE(next, tail));	// advance tail ptr
	    } else {		       // no need to deal with tail
		// read value before CAS, otherwise another dequeue might free the next node
		p = QPTR(next)->value;
		if (qt_cas
		    ((volatile void **)&(q->head), head,
		     QCOMPOSE(next, head)) == head) {
		    break;	       // success!
		}
	    }
	}
    }
    qpool_free(me, qlfqueue_node_pool, QPTR(head));
    return p;
}				       /*}}} */

int qlfqueue_empty(qlfqueue_t * q)
{				       /*{{{ */
    qlfqueue_node_t *head, *tail, *next;

    assert(q != NULL);
    if (q == NULL) {
	return 1;
    }

    while (1) {
	head = (qlfqueue_node_t *) (q->head);
	tail = (qlfqueue_node_t *) (q->tail);
	next = (qlfqueue_node_t *) (QPTR(head)->next);
	if (head == q->head) {	       // are head, tail, and next consistent?
	    if (QPTR(head) == QPTR(tail)) {	// is queue empty or tail falling behind?
		if (QPTR(next) == NULL) {	// queue is empty!
		    return 1;
		} else {	       // tail falling behind (queue NOT empty)
		    return 0;
		}
	    } else {		       // queue is NOT empty and tail is NOT falling behind
		return 0;
	    }
	}
    }
}				       /*}}} */
