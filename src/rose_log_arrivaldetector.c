// a logrithmic self-cleaning(???I hope it will be) arrive first detector
// -- requires the size to be knownn
// Follows a strategy based on the barrier lock - 5/25/09 akp

#include <stddef.h>		       // for size_t (C89)
#include <stdlib.h>		       // for calloc()
#include <qthread/qthread-int.h>       // for int64_t
#include "qthread_innards.h"	       // for qthread_debug()

#include <qthread/qloop.h>
#include "qt_barrier.h"
#include "qthread_asserts.h"

/* Types */
typedef struct qt_arrive_first_s {
    size_t activeSize;		// size of barrier
    size_t allocatedSize;	// lowest size power of 2 equal or bigger than barrier -- for allocations
    int doneLevel;		// height of the tree
    char arriveFirstDebug;	// flag to turn on internal printf debugging
    volatile int64_t **present;	// array of counters to track who's arrived
    volatile int64_t **value;	// array of values to return

} qt_arrive_first_t;
struct guided_s {
    int64_t upper;
    int64_t lower;
    int64_t stride;
};

/* Global Variables
 * (should be static)
 */
static struct guided_s guided = { 1, 1, 1 };
static qt_arrive_first_t *MArrFirst = NULL;

static void qtar_internal_initialize_fixed(
    qt_arrive_first_t * b,
    size_t size,
    int debug);

static void qtar_internal_initialize_fixed(
    qt_arrive_first_t * b,
    size_t size,
    int debug)
{				       /*{{{ */
    int i;
    int depth = 1;
    int temp = size;

    assert(b);
    b->activeSize = size;
    //    b->arriveFirstDebug = (char)debug;
    b->arriveFirstDebug = 0;

    if (size < 1) {
	return;
    }
    // compute size of barrier arrays
    temp >>= 1;
    while (temp) {
	temp >>= 1;
	depth++;		       // how many bits set
    }

    b->doneLevel = depth;
    b->allocatedSize = (2 << depth);

    // allocate and init upLock and downLock arrays
    b->present = calloc(depth, sizeof(int64_t));
    b->value = calloc(depth, sizeof(int64_t));
    for (i = 0; i <= depth; i++) {
	b->present[i] = calloc(b->allocatedSize, sizeof(int64_t));
	b->value[i] = calloc(b->allocatedSize, sizeof(int64_t));
    }
}				       /*}}} */

qt_arrive_first_t *qt_arrive_first_create(
    int size,
    qt_barrier_btype type,
    int debug)
{				       /*{{{ */
    qt_arrive_first_t *b = calloc(1, sizeof(qt_arrive_first_t));

    qthread_debug(ALL_CALLS,
		  "qt_arrive_first_create:size(%i), type(%i), debug(%i): begin\n",
		  size, (int)type, debug);
    assert(b);
    if (b) {
	assert(type == REGION_BARRIER);
	switch (type) {
	    case REGION_BARRIER:
		qtar_internal_initialize_fixed(b, size, debug);
		break;
	    default:
		printf("qt_arrive_first must be of type REGION_BARRIER\n");
		break;
	}
    }
    return b;
}				       /*}}} */

void qt_arrive_first_destroy(
    qt_arrive_first_t * b)
{				       /*{{{ */
    assert(b);
    if (b->present) {
	int i;
	for (i = 0; i <= b->doneLevel; i++) {
	    if (b->present[i]) {
		free((void *)(b->present[i]));
		b->present[i] = NULL;
	    }
	}
	free((void *)(b->present));
	b->present = NULL;
    }
    if (b->value) {
	int i;
	for (i = 0; i <= b->doneLevel; i++) {
	    if (b->present[i]) {
		free((void *)(b->value[i]));
		b->value[i] = NULL;
	    }
	}
	free((void *)(b->value));
	b->value = NULL;
    }
    free(b);
}				       /*}}} */


static int64_t qtar_internal_up(
    qt_arrive_first_t * b,
    int myLock,
    int level);

// walk up the psuedo barrier -- waits if neighbor beat you and
// value has not arrived yet.

qqloop_handle_t *qt_loop_rose_queue_create(
    int64_t rt,
    int64_t p,
    int64_t r);

static int64_t qtar_internal_up(
    qt_arrive_first_t * b,
    int myLock,
    int level)
{				       /*{{{ */
    // compute neighbor node at this level
    int64_t t = 0;
    int mask = 1 << level;
    int pairedLock = myLock ^ mask;
    int nextLevelLock = (myLock < pairedLock) ? myLock : pairedLock;
    char debug = b->arriveFirstDebug;
    assert(b->activeSize > 1);
    if (debug) {
	printf
	    ("on lock %d paired with %d level %d lock value %ld  paired %ld\n",
	     myLock, pairedLock, level, b->present[level][myLock],
	     b->present[level][pairedLock]);
    }
    // my pair is out of range don't wait for it
    if (pairedLock > b->activeSize) {
	// continue up
	if (level != b->doneLevel) {
	    t = qtar_internal_up(b, nextLevelLock, level + 1);
	    b->value[level][myLock] = t;
	} else if (myLock == 0) {
	    if (debug) {
		printf("Lock arrived %d\n", myLock);
	    }
	    t = (int64_t) qt_loop_rose_queue_create(guided.lower,
						    guided.upper,
						    guided.stride);
	};
	return t;
    }
    // mark me as present
    int lk = 0;
    lk = qthread_incr(&b->present[level][nextLevelLock], 1);

    if (lk == 0) {		       // I'm first continue up
	if ((level + 1) <= b->doneLevel) {	// done? -- more to check 
	    t = qtar_internal_up(b, nextLevelLock, level + 1);
	    b->value[level][nextLevelLock] = t;
	}
    } else {			       // someone else is first
	// check to see if value has arrived  -- wait for it
	while ((t = b->value[level][nextLevelLock]) == 0) {	// my pair is not here yet  
	    // gate not lock -- mutex safe?
	}
	b->value[level][myLock] = t;
	b->value[level][nextLevelLock] = 0;
	b->present[level][nextLevelLock] = 0;
    }
    return t;
}				       /*}}} */

void cleanArriveFirst(
    )
{
}


// actual arrive first entry point

int64_t qt_arrive_first_enter(
    qt_arrive_first_t * b,
    qthread_shepherd_id_t shep)
{				       /*{{{ */
    int64_t t = qtar_internal_up(MArrFirst, shep, 0);
    return t;
}				       /*}}} */


int64_t qt_global_arrive_first(
    const qthread_shepherd_id_t shep)
{				       /*{{{ */
    int64_t t = qtar_internal_up(MArrFirst, shep, 0);
    return t;
}				       /*}}} */


// allow initization from C
void qt_global_arrive_first_init(
    int size,
    int debug)
{				       /*{{{ */
    if (MArrFirst == NULL) {
	extern int cnbWorkers;
	extern double cnbTimeMin;
	cnbWorkers = qthread_num_shepherds();
	cnbTimeMin = 1.0;
	MArrFirst = qt_arrive_first_create(size, REGION_BARRIER, debug);
	assert(MArrFirst);
    }
}				       /*}}} */

void qt_global_arrive_first_destroy(
    void)
{				       /*{{{ */
    if (MArrFirst) {
	qt_arrive_first_destroy(MArrFirst);
	MArrFirst = NULL;
    }
}				       /*}}} */
