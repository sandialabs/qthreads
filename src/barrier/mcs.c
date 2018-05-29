#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <stdlib.h>
#include <stdio.h>

/* System Compatibility Header */
#include "qthread-int.h"

/* Public Headers */
#include "qthread/qthread.h"
#include "qthread/barrier.h"

/* Internal Headers */
#include "qt_barrier.h"
#include "qt_atomics.h"
#include "qt_mpool.h"
#include "qt_visibility.h"
#include "qt_initialized.h" // for qthread_library_initialized
#include "qt_debug.h"
#include "qt_asserts.h"
#include "qt_subsystems.h"

/* so this is a per qthread barrier */
typedef struct qt_barrier_s qt_barrier_s;
struct qt_barrier_s {
	uint8_t parentsense;
	uint8_t *parent;
	uint8_t *children[2];

	union {
		uint8_t single[4];
		uint32_t all;
	} havechild;

	union {
		uint8_t single[4];
		uint32_t all;
	} childnotready;

	uint8_t sense;
	uint8_t dummy;
	unsigned int id;

	uint8_t *n;
	uint8_t *nodes;
};

static union {
    qt_mpool pool;
    void    *vp;
} fbp;
QTHREAD_CASLOCK_EXPLICIT_DECL(fbp_caslock)

static qt_barrier_t * global_barrier = NULL;

#ifndef UNPOOLED
static void cleanup_barrier(void)
{

}

#endif

void INTERNAL qt_barrier_internal_init(void)
{
}

qt_barrier_t API_FUNC *qt_barrier_create(size_t           max_threads,
                                         qt_barrier_btype Q_UNUSED(type))
{
    qt_barrier_s *b;
		qt_barrier_s *nodes;
		/* include nodes */
    b = MALLOC(2*max_threads*sizeof(struct qt_barrier_s));
		nodes = b->nodes;
		/* so where I am getting nodes from? */
		/* how do I know what my number is when I enter? */
		for (size_t i = 0; i < max_threads; i++) {
			b[i].id = i;
			b[i].n = &nodes[i];
			/* init barrier */
			qthread_debug ("init for %u\n", i);
			for (size_t j = 0; j < 4; j++) {
				nodes[i].havechild.single[j] = (4*i+j < (max_threads-1)) ? 1 : 0;
				qthread_debug ("havechild[%u]=%i\n", j, nodes[i].havechild.single[j]);
			}
			nodes[i].parent = (i != 0) ? &nodes[(i-1)/4].childnotready.single[(i-1)%4] : &nodes[i].dummy;
			if (i != 0) {
				qthread_debug ("parent = nodes[%u].childnotready[%u]\n", (i-1)/4, (i-1)%4);
			}
			nodes[i].children[0] = (2*i+1 < max_threads) ? &nodes[2*i+1].parentsense : &nodes[i].dummy;
			if (2*i+1 < max_threads) {
				qthread_debug ("children[0] = nodes[%u]\n", 2*i+1);
			}
			nodes[i].children[1] = (2*i+2 < max_threads) ? &nodes[2*i+2].parentsense : &nodes[i].dummy;
			if (2*i+2 < max_threads) {
				qthread_debug ("children[1] = nodes[%u]\n", 2*i+2);
			}
			nodes[i].childnotready.all = nodes[i].havechild.all;
			nodes[i].parentsense = 0x0;
			nodes[i].sense = 0x1;
			nodes[i].dummy = 0x0;
			nodes[i].id = i;
		}

    return b;
}

void API_FUNC qt_barrier_enter_id(qt_barrier_t *b,
                                  size_t        Q_UNUSED(shep))
{
    qt_barrier_enter(b);
}

void API_FUNC qt_barrier_enter(qt_barrier_t *b)
{
    aligned_t waiters;
		qt_barrier_t *n;

    assert(qthread_library_initialized);
    qassert_retvoid(b);
    /* so what are we doing here?
       we need to see how this is working.
     */
		n = b->nodes[qthread_worker(NULL)];
		/* this should be a feb. */
		qthread_readFF(NULL, n->childnotready.all);
		n->childnotready.all = n->havechild.all;
		/* byte stores are atomic on amd64, this should probably be fixed later. */
		*n->parent = 0x0;
		/* is there a way of waiting on a qthread feb? */
		if (n->id != 0) {
			while (n->parentsense != n->sense) {
				qthread_yield ();
			}
		}
		*n->children[0] = n->sense;
		*n->children[1] = n->sense;
		n->sense = !n->sense;

}

void API_FUNC qt_barrier_resize(qt_barrier_t *restrict b,
                                size_t                 new_size)
{
}

void API_FUNC qt_barrier_destroy(qt_barrier_t *b)
{
#ifndef UNPOOLED
#endif
#ifndef UNPOOLED
#else
    FREE(b, sizeof(struct qt_barrier_t));
#endif
}

void qt_global_barrier(void)
{
    assert(global_barrier);
    qt_barrier_enter(global_barrier);
}

void qt_global_barrier_init(size_t size,
                                     int    qthread_debug)
{
    if (global_barrier == NULL) {
        global_barrier = qt_barrier_create(size, 0);
        assert(global_barrier);
    }
}

void qt_global_barrier_destroy(void)
{
    if (global_barrier) {
        qt_barrier_destroy(global_barrier);
        global_barrier = NULL;
    }
}

void qt_global_barrier_resize(size_t size)
{
    qt_barrier_resize(global_barrier, size);
}

/* vim:set expandtab: */
