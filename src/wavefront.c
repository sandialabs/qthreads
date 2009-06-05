#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>		       /* for malloc */

#include <qthread_asserts.h>

#include <qthread/qthread.h>
#include <qthread/qdqueue.h>
#include <qthread/wavefront.h>

struct cacheline_t {
    volatile aligned_t i;
    char pad[QTHREAD_CACHELINE_BYTES-sizeof(aligned_t)];
};

struct qt_wave_wargs {
    qdqueue_t *const work_queue;
    volatile aligned_t *restrict const no_more_work;
    volatile aligned_t *restrict const donecount;
    wave_f func;
    qarray *restrict const *const R;
    size_t maxcols;
    struct cacheline_t* colprogress;
};

struct qt_wave_workunit {
    size_t origrow, startrow, endrow, col;
};

/* to avoid compiler bugs regarding volatile... */
static Q_NOINLINE volatile aligned_t *volatile *vol_id_ap(volatile aligned_t *
							  volatile *ptr)
{
    return ptr;
}
static Q_NOINLINE aligned_t vol_read_a(volatile aligned_t * ptr)
{
    return *ptr;
}
static Q_NOINLINE volatile aligned_t *vol_id_a(volatile aligned_t * ptr)
{
    return ptr;
}

#define _(x) (*vol_id_ap(&(x)))

static void qt_wave_worker(qthread_t * me, struct qt_wave_wargs *const arg)
{
    qarray *restrict const *const R = arg->R;

    while (1) {
	struct qt_wave_workunit *const wu =
	    qdqueue_dequeue(me, arg->work_queue);
	if (wu == NULL) {
	    if (vol_read_a(arg->no_more_work)) {
		qthread_incr(arg->donecount, 1);
		break;
	    }
	    qthread_yield(me);
	} else {
	    const size_t col = wu->col;
	    size_t row;
	    int requeued = 0;

	    /* step 1: call the user function on the input */
	    for (row = wu->startrow; row < wu->endrow; row++) {
		void *left = qarray_elem_nomigrate(R[col - 1], row);

		/* check to see if the left is ready */
		if (col == 1 ||
#ifdef QTHREAD_FEBS_ARE_FAST
		    qthread_feb_status(left)
#else
		    vol_read_a(&(arg->colprogress[col - 1].i)) >= row
#endif
		    ) {
		    void *ptr = qarray_elem_nomigrate(R[col], row);

		    /* we can assume that leftbelow is ready (because left is ready) */
		    void *leftbelow =
			qarray_elem_nomigrate(R[col - 1], row - 1);
		    /* we can further assume that below is ready (either
		     * because this work unit was queued or because we just
		     * computed it) */
		    void *below = qarray_elem_nomigrate(R[col], row - 1);

		    arg->func(left, leftbelow, below, ptr);
#ifdef QTHREAD_FEBS_ARE_FAST
		    qthread_fill(me, ptr);
#else
		    /* this is assumed to be atomic for a single thread */
		    *vol_id_a(&(arg->colprogress[col].i)) += 1;
#endif
		} else {
		    /* re-queue the work unit */
		    wu->startrow = row;
		    requeued = 1;
		    qdqueue_enqueue(me, arg->work_queue, wu);
		    break;
		}
	    }
	    if (requeued == 0 && row == wu->endrow) {
		/* we're done with this array segment! Huzzah! */
		if (wu->origrow == 1 && col + 1 != arg->maxcols) {
		    /* queue work for the right */
		    struct qt_wave_workunit *wu2 =
			malloc(sizeof(struct qt_wave_workunit));
		    qarray *const right = R[col + 1];

		    wu2->startrow = 1;
		    wu2->origrow = 1;
		    wu2->endrow =
			(right->count >
			 right->segment_size) ? (right->
						 segment_size) : (right->
								  count);
		    wu2->col = col + 1;
		    qdqueue_enqueue_there(me, arg->work_queue, wu2,
					  qarray_shepof(right, 1));
		}
		/* now, we may need to queue the next guy up the chain... */
		if (wu->endrow < R[col]->count) {
		    /* queue next (reuse the work unit structure) */
		    wu->startrow = wu->endrow;
		    wu->origrow = wu->endrow;
		    wu->endrow = wu->startrow + R[col]->segment_size;
		    if (wu->endrow > R[col]->count) {
			wu->endrow = R[col]->count;
		    }
		    qdqueue_enqueue_there(me, arg->work_queue, wu,
					  qarray_shepof(R[col],
							wu->startrow));
		} else {
		    /* freeeeedooooooommmmmm! */
		    if (col != 1) {
			free(wu);
		    }
		    if (col + 1 == arg->maxcols) {
			/* ALL DONE */
			*vol_id_a(arg->no_more_work) = 1;
		    }
		}
	    }
	}
    }
}

/* we must assume that R[0] is full, and that R[*][0] is full as well:
 *
 *  R[0][5] R[1][5] R[2][5] R[3][5] R[4][5] R[5][5]
 *  R[0][4] R[1][4] R[2][4] R[3][4] R[4][4] R[5][4]
 *  R[0][3] R[1][3] R[2][3] R[3][3] R[4][3] R[5][3]
 *  R[0][2] R[1][2] R[2][2] R[3][2] R[4][2] R[5][2]
 *  R[0][1] R[1][1] R[2][1] R[3][1] R[4][1] R[5][1]
 *  R[0][0] R[1][0] R[2][0] R[3][0] R[4][0] R[5][0]
 *
 *  We also require that all qarrays in R have the same internal dimensions
 *  (segment size, unit size, etc.)
 *
 */
void qt_wavefront(qarray * restrict const *const R, size_t cols, wave_f func)
{
    const qthread_shepherd_id_t maxsheps = qthread_num_shepherds();
    qthread_shepherd_id_t shep;
    volatile aligned_t no_more_work = 0;
    volatile aligned_t donecount = 0;
    struct qt_wave_wargs wargs =
	{ qdqueue_create(), &no_more_work, &donecount, func, R, cols, NULL };
    qthread_t *const me = qthread_self();
    struct qt_wave_workunit wu = { 0 };

    assert(cols > 1);
    if (cols <= 1) {
	return;
    }
#ifdef QTHREAD_FEBS_ARE_FAST
    /* step 1: empty all the non-computed array areas */
#error Implement array emptying
#else
    /* step 1: create an array to record the data completed in each column */
    wargs.colprogress = calloc(cols, sizeof(struct cacheline_t));
#endif

    /* step 2: set up a qdqueue of work */
    /* -- work queue set up as part of initialization stuff, above */

    /* step 3: spawn workers */
    for (shep = 0; shep < maxsheps; shep++) {
	qthread_fork_to((qthread_f) qt_wave_worker, &wargs, NULL, shep);
    }

    /* step 4: queue a job for the lower-left corner */
    wu.startrow = 1;
    wu.origrow = 1;
    wu.endrow =
	(R[1]->count >
	 R[1]->segment_size) ? (R[1]->segment_size) : (R[1]->count);
    wu.col = 1;
    qdqueue_enqueue_there(me, wargs.work_queue, &wu, qarray_shepof(R[1], 1));

    /* step 5: wait for the workers to get done */
    while (vol_read_a(&donecount) < maxsheps) {
	qthread_yield(me);
    }
    qdqueue_destroy(me, wargs.work_queue);
    free((void *)wargs.colprogress);
}

void qt_basic_wavefront(int *restrict const *const R, size_t cols,
			size_t rows, wave_f func)
{
    /* assuming R is properly initialized. */
    for (size_t col = 1; col < cols; col++) {
	for (size_t row = 1; row < rows; row++) {
	    func(&(R[col - 1][row]), &(R[col - 1][row - 1]),
		 &(R[col][row - 1]), &(R[col][row]));
	}
    }
}
