#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// a logrithmic self-cleaning barrier -- requires the size to be known  
// Follows a strategy that the MTA used - so might work better if it used the
// full/empty implementation, but orginal developement predated it's inclusion
// here and had no available F/E implementation - 12/17/09 akp

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "qt_barrier.h"
#include "qthread_asserts.h"
#include "qt_atomics.h"
#include <qthread/qthread.h>
#include <qthread_innards.h>

struct qt_barrier_s
{
    int count;			// size of barrier
    size_t activeSize;		// size of barrier (KBW: redundant???)
    size_t allocatedSize;	// lowest size power of 2 equal or bigger than barrier -- for allocations
    int doneLevel;		// height of the tree
    char barrierDebug;		// flag to turn on internal printf debugging
    volatile int64_t *upLock;	// array of counters to track number of people that have arrived
    volatile int64_t *downLock;	// array of counters that allows threads to leave

} /* qt_barrier_t */ ;

static void qtb_internal_initialize_variable(qt_barrier_t * b, size_t size,
					     int debug);
static void qtb_internal_initialize_fixed(qt_barrier_t * b, size_t size,
					  int debug);

static void qtb_internal_up(qt_barrier_t * b, int myLock, int64_t val,
			    int level);
static void qtb_internal_down(qt_barrier_t * b, int mylock, int level);

/* global barrier */
qt_barrier_t *MBar = NULL;

void qt_barrier_destroy(qt_barrier_t * b)
{				       /*{{{ */
    assert(b);
    if (b->upLock) {
	free((void *)(b->upLock));
    }
    if (b->downLock) {
	free((void *)(b->downLock));
    }
    free(b);
}				       /*}}} */

qt_barrier_t *qt_barrier_create(int size, qt_barrier_btype type, int debug)
{				       /*{{{ */
    qt_barrier_t *b = calloc(1, sizeof(qt_barrier_t));

    qthread_debug(ALL_CALLS, "size(%i), type(%i), debug(%i): begin\n", size,
		  (int)type, debug);
    assert(b);
    if (b) {
	assert(type == REGION_BARRIER);
	switch (type) {
	    case REGION_BARRIER:
		qtb_internal_initialize_fixed(b, size, debug);
		break;
	    default:
		qtb_internal_initialize_variable(b, size, debug);
		break;
	}
    }
    return b;
}				       /*}}} */

static void qtb_internal_initialize_variable(qt_barrier_t * b, size_t size,
					     int debug)
{				       /*{{{ */
    assert(0);
    printf("Loop barrier not implemented yet\n");
}				       /*}}} */

static void qtb_internal_initialize_fixed(qt_barrier_t * b, size_t size,
					  int debug)
{				       /*{{{ */
    int depth = 1;
    int temp = size;

    assert(b);
    b->activeSize = size;
    b->barrierDebug = (char)debug;

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
    b->upLock = calloc(b->allocatedSize, sizeof(int64_t));
    b->downLock = calloc(b->allocatedSize, sizeof(int64_t));

    for (size_t i = b->activeSize + 1; i < b->allocatedSize; i++) {
	b->upLock[i] = INT64_MAX;      // 64 bit  -- should never overflow
    }
}				       /*}}} */

// dump function for debugging -  print barrier array contents
void qt_barrier_dump(qt_barrier_t * b, enum dumpType dt)
{				       /*{{{ */
    size_t i, j;
    const size_t activeSize = b->activeSize;

    if ((dt == UPLOCK) || (dt == BOTHLOCKS)) {
	printf("upLock\n");
	for (j = 0; j < activeSize; j += 8) {
	    for (i = 0; ((i < 8) && ((j + i) <= activeSize)); i++) {
		printf("%ld ", (long int)b->upLock[j + i]);
	    }
	    printf("\n");
	}
    }
    if ((dt == DOWNLOCK) || (dt == BOTHLOCKS)) {
	printf("downLock\n");
	for (j = 0; j < activeSize; j += 8) {
	    for (i = 0; ((i < 8) && ((j + i) <= activeSize)); i++) {
		printf("%ld ", (long int)b->downLock[j + i]);
	    }
	    printf("\n");
	}
    }
    return;
}				       /*}}} */

// walk down the barrier -- releases all locks in subtree below myLock
//    level -- how high in the tree is this node
static void qtb_internal_down(qt_barrier_t * b, int myLock, int level)
{				       /*{{{ */
    assert(b->activeSize > 1);
    for (int i = level; i >= 0; i--) {
	int mask = 1 << i;
	int pairedLock = myLock ^ mask;
	if (pairedLock <= b->activeSize) {	// my pair is in of range
	    if (pairedLock > myLock) {
		b->downLock[pairedLock]++;	// mark me as released
		if (b->barrierDebug) {
		    printf("\t down lock %d level %d \n", pairedLock, i);
		}
	    }
	} else {		       // out of range -- continue
	}
    }
}				       /*}}} */

// walk up the barrier -- waits for neighbor lock at each level of the tree
//   when both arrive the lower thread number climbs up the tree and the
//   higher number waits for the down walk to release.  When all of the nodes arrive
//   Thread 0 starts the walk down (after executing functions to resync for loop
//   implementation).  Any node that is waiting release at a level other than the
//   leaves, releases its subtree
static void qtb_internal_up(qt_barrier_t * b, int myLock, int64_t val,
			    int level)
{				       /*{{{ */
    // compute neighbor node at this level
    int mask = 1 << level;
    int pairedLock = myLock ^ mask;
    char debug = b->barrierDebug;
    assert(b->activeSize > 1);
    if (debug) {
	printf("on lock %d paired with %d level %d val %ld\n", myLock,
	       pairedLock, level, (long int)val);
    }
    if (pairedLock > b->activeSize) { // my pair is out of range don't wait for it
	(void)qthread_incr(&b->upLock[myLock], 1);	// mark me as present
	while (b->downLock[myLock] != val) ;	// KBW: XXX: not mutex safe
	if (debug) {
	    printf("released (no pair) lock %d level %d val %ld\n", myLock,
		   level, (long int)val);
	}
	if (level != 0)
	    qtb_internal_down(b, myLock, level);	// everyone is here and I have people to release
	return;			       // done
    }

    if (pairedLock < myLock) {	       // I'm higher -- wait for release
	(void)qthread_incr(&b->upLock[myLock], 1);	// mark me as present
	if (debug) {
	    printf
		("about to wait on lock %d paired with %d level %d val %ld\n",
		 myLock, pairedLock, level, (long int)val);
	}
	while (b->downLock[myLock] != val) {	// my pair is not here yet (KBW:XXX: not mutex safe)
	}
	if (debug) {
	    printf("released lock %d level %d val %ld\n", myLock, level,
		   (long int)val);
	}
	if (level != 0)
	    qtb_internal_down(b, myLock, level);	// everyone is here and I have people to release
	return;			       // done
    } else {			       // I'm lower -- wait for pair and continue up
	if (debug) {
	    printf("wait lock %d for %d level %d val %ld\n", myLock,
		   pairedLock, level, (long int)val);
	}
	while (b->upLock[pairedLock] < val) {	// my pair is not here yet (KBW:XXX: not mutex safe)
	}
	if (debug) {
	    printf("continue on %d level %d val %ld\n", myLock, level,
		   (long int)val);
	}
    }

    if ((level + 1 < b->doneLevel) || (myLock != 0)) {	// not done?  yes
	qtb_internal_up(b, myLock, val, level + 1);
    } else {			       // done -- start release
	(void)qthread_incr(&b->upLock[myLock], 1);	// mark me as present
	(void)qthread_incr(&b->downLock[myLock], 1);	// mark me as released
	if (debug) {
	    printf("\t start down lock %d level %d \n", myLock, level);
	}
	qtb_internal_down(b, myLock, level);	// everyone is here
    }
}				       /*}}} */

// actual barrier entry point

void qt_barrier_enter(qt_barrier_t * b, qthread_shepherd_id_t shep)
{				       /*{{{ */
    // should be dual versions  1) all active threads barrier
    //                          2) all active streams

    // only dealing with (1) for first pass for now
    int64_t val = b->upLock[shep] + 1;

    if (b->activeSize < 1)
	return;
    qtb_internal_up(b, shep, val, 0);
}				       /*}}} */

// used indirectly by omp barrier calls (initially - others I hope)
// akp 7/24/09
#define QT_GLOBAL_LOGBARRIER
#ifdef QT_GLOBAL_LOGBARRIER
void qt_global_barrier(const qthread_t * me)
{				       /*{{{ */
    const qthread_shepherd_id_t shep = qthread_shep(me);
    qt_barrier_enter(MBar, shep);
    //  now execute code on one thread that everyone needs to see -- should be
    //     at middle of barrier but does not seem to work there -- so here with double barrier
    //     blech.  akp -2/9/10
    qthread_reset_forCount(qthread_self());	// for loop reset on each thread
    qt_barrier_enter(MBar, shep);
    return;
}				       /*}}} */

// allow barrer initization from C
void qt_global_barrier_init(int size, int debug)
{				       /*{{{ */
    if (MBar == NULL) {
	MBar = qt_barrier_create(size, REGION_BARRIER, debug);
	assert(MBar);
    }
}				       /*}}} */

void qt_global_barrier_destroy()
{				       /*{{{ */
    if (MBar) {
	qt_barrier_destroy(MBar);
	MBar = NULL;
    }
}				       /*}}} */
#endif
