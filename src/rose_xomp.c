//  include libxomp.h from ROSE implementation as the full list of functions needed to
//  support OpenMP 3.0 with Rose.

//  The following definations are required for IS to run (implement first)

#include <stdlib.h>		       // for malloc()
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include <qthread/qloop.h>	       // for qt_loop_f
#include "qthread_innards.h"	       // for qthread_debug()
#include "qt_barrier.h"		       // for qt_global_barrier()
#include "rose_log_arrivaldetector.h"  // for qt_global_arrive_first()
#include "qloop_innards.h"	       // for qtrose_loop_handle_t and qloop_internal_computeNextBlock()

#include "rose_xomp.h"

#define bool unsigned char

//Runtime library initialization routine
void XOMP_init(
    int argc,
    char **argv)
{
    qthread_initialize();
    return;
}

// Runtime library termination routine
void XOMP_terminate(
    int exitcode)
{
    qthread_finalize();
    return;
}

void XOMP_parallel_start(
    void (*func) (void *),
    void *data,
    unsigned numThread)
{
    qthread_shepherd_id_t parallelWidth = qthread_num_shepherds();
    qt_loop_step_f f = (qt_loop_step_f) func;
    qt_parallel(f, parallelWidth, data);
    return;
}

void XOMP_parallel_end(
    void)
{
    //intentionally blank
    return;
}

#define ROSE_TIMED 1
volatile qtrose_loop_handle_t *array[64];
int lastBlock[64];
int smallBlock[64];

qtimer_t loopTimer;
int firstTime[64];

void XOMP_loop_guided_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
    qthread_t *const me = qthread_self();
    volatile qtrose_loop_handle_t *qqhandle = NULL;

    /*
     * guided.upper = upper;
     * guided.lower = lower;
     * guided.stride = stride;
     */

    loopTimer = qtimer_create();
    int myid = qthread_shep(me);

    qqhandle =
	(volatile qtrose_loop_handle_t *)qt_global_arrive_first(qthread_shep(me));

    array[myid] = qqhandle;
    firstTime[myid] = 1;
    smallBlock[myid] = qqhandle->assignStop - qqhandle->assignNext;
    return;
}


bool XOMP_loop_guided_start(
    long startLower,
    long startUpper,
    long stride,
    long chunk_size,
    long *returnLower,
    long *returnUpper)
{
    double time;
    qthread_t *const me = qthread_self();
    int myid = qthread_shep(me);
    volatile qtrose_loop_handle_t *loop = array[myid];	// from init;
    if (!firstTime[myid]) {
	qtimer_stop(loopTimer);
	time = qtimer_secs(loopTimer);
	if (time > 7.5e-7 && smallBlock[myid] >= lastBlock[myid]) {
	    smallBlock[myid] = lastBlock[myid];
	}
    } else {
	time = 1.0;
	firstTime[myid] = 0;
	smallBlock[myid] = 5000;
    }

    int dynamicBlock = qloop_internal_computeNextBlock(smallBlock[myid], time, loop);

    aligned_t iterationNumber = qthread_incr(&loop->assignNext, dynamicBlock);
    *returnLower = iterationNumber;
    aligned_t iterationStop = iterationNumber + dynamicBlock;
    if (iterationStop >= loop->assignStop) {
	iterationStop = loop->assignStop;
	if (iterationNumber >= loop->assignStop) {
	    *returnLower = loop->assignStop;
	    *returnUpper = iterationStop;
	    return 0;
	}
    }
    lastBlock[myid] = dynamicBlock;
    *returnUpper = iterationStop;

    qthread_debug(ALL_DETAILS,
		  "limit %10d lower %10d upper %10d block %10d smallBlock %10d id %d\n",
		  loop->assignDone, *returnLower, *returnUpper, dynamicBlock,
		  smallBlock[myid], myid);


    qtimer_start(loopTimer);

    return (dynamicBlock > 0);
}

bool XOMP_loop_guided_next(
    long *returnLower,
    long *returnUpper)
{
    int ret =
	XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
    return ret;
}

void XOMP_loop_end(
    void)
{
    //intentionally blank
    qtimer_stop(loopTimer);
    qthread_t *const me = qthread_self();
    qt_global_barrier(me);	       // need barrier or timeout in qt_loop_inner kills performance
}

void XOMP_loop_end_nowait(
    void)
{
    //intentionally blank
    qtimer_stop(loopTimer);
    qthread_t *const me = qthread_self();
    qt_global_barrier(me);	       // need barrier or timeout in qt_loop_inner kills performance
}

void XOMP_barrier(
    void)
{
    qthread_t *const me = qthread_self();
    qt_global_barrier(me);
}

void XOMP_atomic_start(
    void)
{
}

void XOMP_atomic_end(
    void)
{
}


// needed for full OpenMP 3.0 support

void XOMP_loop_ordered_guided_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
}

bool XOMP_loop_ordered_guided_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
    return 0;
}

bool XOMP_loop_ordered_guided_next(
    long *a,
    long *b)
{
    return 0;
}

void XOMP_task(
    void (*a) (void *),
    void *b,
    void (*c) (void *,
	       void *),
    long d,
    long e,
    bool f,
    unsigned g)
{
}

void XOMP_taskwait(
    void)
{
}


// scheduler functions - guided scheduling (default for Qthreads)
void XOMP_loop_static_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
}

void XOMP_loop_dynamic_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
}

void XOMP_loop_runtime_init(
    int lower,
    int upper,
    int stride)
{
}

//ordered case
void XOMP_loop_ordered_static_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
}

void XOMP_loop_ordered_dynamic_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
}

void XOMP_loop_ordered_runtime_init(
    int lower,
    int upper,
    int stride)
{
}






// rest of the functions


// omp ordered directive
void XOMP_ordered_start(
    void)
{
}

void XOMP_ordered_end(
    void)
{
}

// if (start), 
// mostly used because of gomp, omni will just call  XOMP_loop_xxx_next(){}
bool XOMP_loop_static_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
    return 0;
}

bool XOMP_loop_dynamic_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
    return 0;
}

bool XOMP_loop_runtime_start(
    long a,
    long b,
    long c,
    long *d,
    long *e)
{
    return 0;
}

bool XOMP_loop_ordered_static_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
    return 0;
}

bool XOMP_loop_ordered_dynamic_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
    return 0;
}

bool XOMP_loop_ordered_runtime_start(
    long a,
    long b,
    long c,
    long *d,
    long *e)
{
    return 0;
}

// next
bool XOMP_loop_static_next(
    long *a,
    long *b)
{
    return 0;
}

bool XOMP_loop_dynamic_next(
    long *a,
    long *b)
{
    return 0;
}

bool XOMP_loop_runtime_next(
    long *a,
    long *b)
{
    return 0;
}

bool XOMP_loop_ordered_static_next(
    long *a,
    long *b)
{
    return 0;
}

bool XOMP_loop_ordered_dynamic_next(
    long *a,
    long *b)
{
    return 0;
}

bool XOMP_loop_ordered_runtime_next(
    long *a,
    long *b)
{
    return 0;
}

//--------------end of  loop functions 

void XOMP_critical_start(
    void **data)
{
}

void XOMP_critical_end(
    void **data)
{
}

bool XOMP_single(
    void)
{
    return 0;
}

bool XOMP_master(
    void)
{
    return 0;
}



// flush without variable list
void XOMP_flush_all(
    void)
{
}

// omp flush with variable list, flush one by one, given each's start address and size
void XOMP_flush_one(
    char *startAddress,
    int nbyte)
{
}
