//  include libxomp.h from ROSE implementation as the full list
//  of functions needed to support OpenMP 3.0 with Rose.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>		       // for malloc()
#include <stdio.h>


#include <time.h>                      // for omp_get_wtick/wtime
#if defined(CLOCK_REALTIME) || defined(CLOCK_MONOTONIC)
// Otherwise get tick resolution from sysconf
#include <unistd.h>                    // for omp_get_wtick
#endif
#include <string.h>                    // for strcmp

#include "qthread/qthread.h"
#include "qthread/qtimer.h"
#include "qt_barrier.h"	               // for qt_global_barrier
#include "qt_arrive_first.h"           // for qt_global_arrive_first
#include "qthread/qloop.h"	       // for qt_loop_f
#include "qthread_innards.h"	       // for qthread_debug()
#include "qloop_innards.h"	       // for qqloop_handle_t
#include <qthread/feb_barrier.h>
#include <rose_xomp.h>

#if defined(__i386__) || defined(__x86_64__)
#define USE_RDTSC 1
#endif

#define bool unsigned char
#define TRUE 1
#define FALSE 0

/* XXX: KBW: fixes a compiler warning */
void omp_set_nested (int val);

typedef enum xomp_nest_level{NO_NEST=0, ALLOW_NEST, AUTO_NEST}xomp_nest_level_t;

//
// XOMP_Status
//
// Internal structure used to maintain state of XOMP system
typedef struct XOMP_Status
{
  bool inside_xomp_parallel;
  xomp_nest_level_t allow_xomp_nested_parallel;
  int64_t xomp_nested_parallel_level;
  clock_t start_time;
  qthread_shepherd_id_t num_omp_threads;
  int64_t dynamic;
  enum qloop_handle_type runtime_sched_option;
  aligned_t atomic_lock;
} XOMP_Status;
static XOMP_Status xomp_status;

static void XOMP_Status_init(XOMP_Status *);
static void set_inside_xomp_parallel(XOMP_Status *, bool);
static bool get_inside_xomp_parallel(XOMP_Status *);
static int incr_inside_xomp_nested_parallel(XOMP_Status *p_status);
static int decr_inside_xomp_nested_parallel(XOMP_Status *p_status);
static void set_xomp_dynamic(int, XOMP_Status *);
static int64_t get_xomp_dynamic(XOMP_Status *);
static void xomp_set_nested(XOMP_Status *, bool val);
static xomp_nest_level_t xomp_get_nested(XOMP_Status *);

// Initalize the structure to a known state
static void XOMP_Status_init(
    XOMP_Status *p_status)
{
  p_status->inside_xomp_parallel = FALSE;
  p_status->allow_xomp_nested_parallel = 0;
  p_status->xomp_nested_parallel_level = NO_NEST;
  p_status->start_time = clock();
  p_status->num_omp_threads = qthread_num_shepherds();
  p_status->dynamic = 0;
  p_status->runtime_sched_option = GUIDED_SCHED; // Set default scheduling type
  p_status->atomic_lock = 0;
}

// Get atomic lock address location
static aligned_t *get_atomic_lock(
    XOMP_Status *p_status)
{
	return (&(p_status->atomic_lock));
}

// Set runtime schedule option
static void set_runtime_sched_option(
    XOMP_Status *p_status,
    enum qloop_handle_type sched_option)
{
    p_status->runtime_sched_option = sched_option;
}

// Get runtime schedule option
static enum qloop_handle_type get_runtime_sched_option(
    XOMP_Status *p_status)
{
    return p_status->runtime_sched_option;
}

// Set status of inside parallel section (TRUE || FALSE)
static void set_inside_xomp_parallel(
    XOMP_Status *p_status,
    bool state)
{
  p_status->inside_xomp_parallel = state;
}

// Get status of inside parallel section (return TRUE || FALSE)
static bool get_inside_xomp_parallel(
    XOMP_Status *p_status)
{
  return p_status->inside_xomp_parallel;
}
// increasing level of nesting of OMP parallel regions -- returns new level
int incr_inside_xomp_nested_parallel(XOMP_Status *p_status){
  return  ++p_status->xomp_nested_parallel_level;
}

static void xomp_set_nested (XOMP_Status *p_status, bool val)
{
  p_status->allow_xomp_nested_parallel =  val;
}

static xomp_nest_level_t xomp_get_nested (XOMP_Status *p_status)
{
  return p_status->allow_xomp_nested_parallel;
}


// decrease level of nesting of OMP parallel regions -- returns new level

// if I decr, I turn off nested before last barrier -- a problem 
// need to keep nested set until parallel region ends only then turn it off 
// need second variable (I think)

static int decr_inside_xomp_nested_parallel(XOMP_Status *p_status){
  return  p_status->xomp_nested_parallel_level;
}

// return current level of nesting of OMP parallel regions
static int get_inside_xomp_nested_parallel(XOMP_Status *p_status){
  return  p_status->xomp_nested_parallel_level;
}


// Get wtime used by omp
static double XOMP_get_wtime(
    XOMP_Status *p_status)
{
// Copied from libgomp/config/posix/time.c included in gcc-4.4.4
#if defined(CLOCK_REALTIME) || defined(CLOCK_MONOTONIC)
  struct timespec ts;
# ifdef CLOCK_MONOTONIC
  if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0)
# endif
    clock_gettime (CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
#else
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
#endif
}

// get wtick used by omp
static double XOMP_get_wtick(
    XOMP_Status *p_status)
{
// Copied from libgomp/config/posix/time.c included in gcc-4.4.4
#if defined(CLOCK_REALTIME) || defined(CLOCK_MONOTONIC)
  struct timespec ts;
# ifdef CLOCK_MONOTONIC
  if (clock_getres (CLOCK_MONOTONIC, &ts) < 0)
# endif
    clock_getres (CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
#else
  return 1.0 / sysconf(_SC_CLK_TCK);
#endif
}

// Get dynamic scheduler value
static int64_t get_xomp_dynamic(
    XOMP_Status *p_status)
{
  return p_status->dynamic;
}

// Set dynamic scheduler value
static void set_xomp_dynamic(
    int val,
    XOMP_Status *p_status)
{
  p_status->dynamic = val;
}
//
// END XOMP_Status
//

//
// Setup local variables
//

//
// END Setup local variables 
//

static volatile qt_feb_barrier_t *gb = NULL;
static aligned_t outGate = 0; // lock to control loop creation 

//Runtime library initialization routine
void XOMP_init(
    int argc,
    char **argv)
{
    char *env;  // Used to get Envionment variables
    qthread_initialize();

    XOMP_Status_init(&xomp_status);  // Initialize XOMP_Status

    qthread_t *const me = qthread_self();
    qthread_syncvar_empty(me,&outGate); // initialize lock to control loop creation
    
    // Process special environment variables
    if ((env=getenv("OMP_SCHEDULE")) != NULL) {
      if (!strcmp(env, "GUIDED_SCHED")) set_runtime_sched_option(&xomp_status, GUIDED_SCHED);
       else if (!strcmp(env, "STATIC_SCHED")) set_runtime_sched_option(&xomp_status, STATIC_SCHED);
       else if (!strcmp(env, "DYNAMIC_SCHED")) set_runtime_sched_option(&xomp_status, DYNAMIC_SCHED);
       else { // Environemnt variable set to something else, we're going to abort
         fprintf(stderr, "OMP_SCHEDULE set to '%s' which is not a valid value, aborting.\n", env);
	 abort();
       }
    }
    if ((env=getenv("OMP_NESTED")) != NULL) {
      if (!strcmp(env, "TRUE")) omp_set_nested(ALLOW_NEST);
      else if (!strcmp(env, "FALSE")) omp_set_nested(NO_NEST);
      else {
	fprintf(stderr, "OMP_NESTED should be TRUE or FALSE set to %s which is not a valid value, aborting.\n", env);
	abort();
      }
    }
    else {
      omp_set_nested(NO_NEST);
    }
    return;
}

// Runtime library termination routine
void XOMP_terminate(
    int exitcode)
{
    qthread_finalize();
    return;
}

// start a parallel task
void XOMP_parallel_start(
    void (*func) (void *),
    void *data,
    unsigned numThread)
{
  qthread_t *const me = qthread_self();
  
  if (get_inside_xomp_parallel(&xomp_status)){ // already parallel add to nesting level
    incr_inside_xomp_nested_parallel(&xomp_status);
    if (!xomp_get_nested(&xomp_status)) {  // going nested but not set in environment/nested call
      xomp_set_nested(&xomp_status,AUTO_NEST);
    } 
  }
  set_inside_xomp_parallel(&xomp_status, TRUE);

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  if (gb == NULL) gb = qt_feb_barrier_create(me,qthread_num_workers()); // setup taskwait barrier
  qthread_shepherd_id_t parallelWidth = qthread_num_workers();
#else
  if (gb == NULL) gb = qt_feb_barrier_create(me,qthread_num_shepherds()); // setup taskwait barrier
  qthread_shepherd_id_t parallelWidth = qthread_num_shepherds();
#endif
  qt_loop_f f = (qt_loop_f) func;
  qt_parallel_step(f, parallelWidth, data);

  return;
}

// end a parallel task
void XOMP_parallel_end(
    void)
{
  int nest = decr_inside_xomp_nested_parallel(&xomp_status);
  if (nest == 0){
    set_inside_xomp_parallel(&xomp_status, FALSE);
    if(xomp_get_nested(&xomp_status) == AUTO_NEST) {
      xomp_set_nested(&xomp_status, NO_NEST);
    } 
  }
  return;
}

// helper function to set up parallel loop
#define ROSE_TIMED 1

int64_t arr[100][64];
volatile int64_t arr_level[64];

qqloop_step_handle_t* qqloop_get_value(int id)
{
    qqloop_step_handle_t * ret = NULL;
    while(1) {
       int lev = *(volatile int64_t*)(&arr_level[id]);
       if (*(volatile int*)(&arr[lev][id])){
	   ret = arr[lev][id];
	   break;
       }
    }

    return ret;
}

void qqloop_clear_value(int id)
{
  qthread_t *const me = qthread_self();
  int64_t lev = arr_level[id];
  arr[lev][id] = 0;
  qthread_incr(&arr_level[id], -1);
  return;
}

void qqloop_set_value(qqloop_step_handle_t * qqhandle)
{
  int i;
  int lim;
  //   qthread_t *const me = qthread_self();
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  lim = qthread_num_workers();
#else
  lim = qthread_num_shepherds();
#endif

  for (i = 0; i < lim; i++){
    int64_t lev = qthread_incr(&arr_level[i], 1);
    asm __volatile__("": : :"memory");
    arr[lev+1][i] = (int64_t)qqhandle;
  }
  return;
}


int lastBlock[64];
int smallBlock[64];

qqloop_step_handle_t *qt_loop_rose_queue_create(
    int64_t start,
    int64_t stop,
    int64_t incr)
{
    qqloop_step_handle_t *ret;
    ret = (qqloop_step_handle_t *) malloc(sizeof(qqloop_step_handle_t));

    ret->workers = 0;
    ret->shepherdsActive = 0;
    ret->assignNext = start;
    ret->assignStop = stop;
    ret->assignStep = incr;
    ret->assignDone = stop;

    return ret;
}

// used to compute time to dynamically determine minimum effective block size 
#ifdef USE_RDTSC
#include <stdint.h>
static QINLINE uint64_t rdtsc() {
uint32_t lo, hi;
__asm__ __volatile__ (      // serialize
"xorl %%eax,%%eax \n        cpuid"
::: "%rax", "%rbx", "%rcx", "%rdx");
/* We cannot use "=A", since this would use %rax on x86_64 */
__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
return (uint64_t)hi << 32 | lo;
}

// NOTE: need to replace these magic numbers with #defs 
uint64_t loopTimer[64*16];
#else
qtimer_t loopTimer;
#endif

int firstTime[64];
int currentIteration[64];
int staticStartCount[64];
volatile int orderedLoopCount = 0;
static size_t gate_cnt = 0;

// handle Qthread default openmp loop initialization
void XOMP_loop_guided_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  //  int i;
  qthread_t *const me = qthread_self();
  qqloop_step_handle_t *qqhandle = NULL;
  
  int myid;
#ifdef USE_RDTSC
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_worker_id_t workerid = qthread_worker(&myid,me);
  loopTimer[workerid] = 0;
#else
  myid = qthread_shep(me);
  loopTimer[myid] = 0;
#endif
#else
  loopTimer = qtimer_create();
#endif

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  if (qt_global_arrive_first(workerid,xomp_get_nested(&xomp_status))) {
#else
  if (qt_global_arrive_first(myid,xomp_get_nested(&xomp_status))) {
#endif
    qqhandle  =  qt_loop_rose_queue_create(lower,
					   upper,
					   stride);
    
    qqhandle->chunkSize = 1;
    qqhandle->type = GUIDED_SCHED;
    
    orderedLoopCount = 0;
    
    qqloop_set_value(qqhandle);
    qthread_syncvar_fill(me,&outGate); // open gate 
  }

  else {
    qthread_syncvar_readFF(me, NULL, &outGate); // make sure that this loop is set up
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    qqhandle = qqloop_get_value(workerid);
#else
    qqhandle = qqloop_get_value(myid);
#endif
  }
  
  aligned_t barrierSize = qtar_size() + 1; // returns index value -- need count of waiters
  aligned_t bCnt = qthread_incr(&gate_cnt, 1) + 1; // number waiting + myself
  if(bCnt == barrierSize) {
    qthread_syncvar_empty(me,&outGate);
    qthread_incr(&gate_cnt, -barrierSize);
  }
  
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  firstTime[workerid] = 1;
  currentIteration[workerid] = 1;
  smallBlock[workerid] =  qqhandle->assignStop - qqhandle->assignNext;;
  lastBlock[workerid] =  qqhandle->assignStop - qqhandle->assignNext;
#else
  firstTime[myid] = 1;
  currentIteration[myid] = 1;
  smallBlock[myid] =  qqhandle->assignStop - qqhandle->assignNext;;
  lastBlock[myid] =  qqhandle->assignStop - qqhandle->assignNext;
#endif
  return;
}

int compute_XOMP_block(int block, double time, volatile qqloop_step_handle_t * loop);


int compute_XOMP_block(
    int block,
    double time,
    volatile qqloop_step_handle_t * loop)
{
  int ret = 1;
  switch (loop->type)
    {
    case GUIDED_SCHED: // default schedule -- varient of guided self-schedule to prevent overly small blocks
      {
      ret = qloop_internal_computeNextBlock(block,time,loop);
      break;
      }
    case STATIC_SCHED: // get my next chunk -- used to access same data with same threads over time
      {
      ret = loop->chunkSize;
      break;
      }
    case DYNAMIC_SCHED: // get next available chunk
      {
      ret = loop->chunkSize;
      break;
      }
    case RUNTIME_SCHED: // look at runtime variable to determine which type to use
      // this really should never be hit -- runtime init should pick one of the others
      // default to dynamic because its the shortest to type
      {
      ret = loop->chunkSize;
      break;
      }
    default:
      fprintf(stderr, "compute_XOMP_block invalid loop type %d\n", loop->type);
    }


  return ret;
}


// start Qthreads default openmp loop execution
bool XOMP_loop_guided_start(
    long startLower,
    long startUpper,
    long stride,
    long chunk_size,
    long *returnLower,
    long *returnUpper)
{
    double time;
    qthread_shepherd_id_t myid;
    qthread_t *const me = qthread_self();
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    // both halves nearly the same -- difference workerid for MTS shep id for original
    qthread_worker_id_t workerid = qthread_worker(&myid,me);
    qqloop_step_handle_t *loop = qqloop_get_value(workerid);	// from init;
#ifdef USE_RDTSC
    if (!firstTime[workerid] && (loop->type == GUIDED_SCHED)) {
        uint64_t s = rdtsc();
	time = s - loopTimer[workerid];
	if (time > 10000)
	  smallBlock[myid] = (smallBlock[myid] < lastBlock[workerid]) 
	    ? smallBlock[myid] : lastBlock[workerid];
    } else {
	time = 1.0;
	firstTime[workerid] = 0;
	smallBlock[workerid] = 5000;
    }
#else
    if (!firstTime[workerid] && (loop->type == GUIDED_SCHED)) {
	qtimer_stop(loopTimer);
	time = qtimer_secs(loopTimer);
	if (time > 7.5e-7)
	    smallBlock[workerid] = (smallBlock[workerid] < lastBlock[workerid]) 
	      ? smallBlock[workerid] : lastBlock[workerid];
    } else {
	time = 1.0;
	firstTime[workerid] = 0;
	smallBlock[workerid] = 5000;
    }
#endif
#else
    myid = qthread_shep(me);
    qqloop_step_handle_t *loop = qqloop_get_value(myid);	// from init;
#ifdef USE_RDTSC
    if (!firstTime[myid] && (loop->type == GUIDED_SCHED)) {
        uint64_t s = rdtsc();
	time = s - loopTimer[myid];
	if (time > 10000)
	  smallBlock[myid] = (smallBlock[myid] < lastBlock[myid]) 
	    ? smallBlock[myid] : lastBlock[myid];
    } else {
	time = 1.0;
	firstTime[myid] = 0;
	smallBlock[myid] = 5000;
    }
#else
    if (!firstTime[myid] && (loop->type == GUIDED_SCHED)) {
	qtimer_stop(loopTimer);
	time = qtimer_secs(loopTimer);
	if (time > 7.5e-7)
	    smallBlock[myid] = (smallBlock[myid] < lastBlock[myid]) 
	      ? smallBlock[myid] : lastBlock[myid];
    } else {
	time = 1.0;
	firstTime[myid] = 0;
	smallBlock[myid] = 5000;
    }
#endif
#endif

    int dynamicBlock;
    if (loop->type == GUIDED_SCHED){
      dynamicBlock = qloop_internal_computeNextBlock(smallBlock[myid],time,loop);
    }
    else {
      dynamicBlock = compute_XOMP_block(smallBlock[myid], time, loop);
    }
    aligned_t iterationNumber = qthread_incr(&loop->assignNext, dynamicBlock);
    *returnLower = iterationNumber;
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    currentIteration[workerid] = *returnLower;
#else
    currentIteration[myid] = *returnLower;
#endif
    aligned_t iterationStop = iterationNumber + dynamicBlock;
    if (iterationStop >= loop->assignStop) {
	iterationStop = loop->assignStop;
	if (iterationNumber >= loop->assignStop) {
	    *returnLower = loop->assignStop;
	    *returnUpper = iterationStop;
	    return 0;
	}
    }
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    lastBlock[workerid] = dynamicBlock;
#else
    lastBlock[myid] = dynamicBlock;
#endif
    *returnUpper = iterationStop;

    qthread_debug(ALL_DETAILS,
		  "limit %10d lower %10d upper %10d block %10d smallBlock %10d id %d\n",
		  loop->assignDone, *returnLower, *returnUpper, dynamicBlock,
		  smallBlock[myid], 
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
		  workerid
#else
		  myid
#endif
		  );

#ifdef USE_RDTSC
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    loopTimer[workerid] = rdtsc();
#else
    loopTimer[myid] = rdtsc();
#endif
#else
    qtimer_start(loopTimer);
#endif
    return (dynamicBlock > 0);
}

// get next iteration(if any) for Qthreads default openmp loop execution
bool XOMP_loop_guided_next(
    long *returnLower,
    long *returnUpper)
{
  return XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
}

// Openmp parallel for loop is completed (waits for all to complete)
void XOMP_loop_end(
    void)
{
#ifdef USE_RDTSC
#else
  qtimer_stop(loopTimer);
#endif
  qthread_t *const me = qthread_self();
  XOMP_barrier();            // need barrier or timeout in qt_loop_inner kills performance
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qqloop_clear_value(qthread_worker(NULL,me));
#else
  qqloop_clear_value(qthread_shep(me));
#endif
}

// Openmp parallel for loop is completed --NOTE-- barrier not required by OpenMP
// but needed to keep Qtheads sanity for the moment
void XOMP_loop_end_nowait(
    void)
{
  XOMP_loop_end();
}

// Qthread implementation of a OpenMP global barrier
void walkSyncTaskList(qthread_t *me);
extern int activeParallelLoop;

void XOMP_barrier(void)
{
    qthread_t *const me = qthread_self();
    if ( 
      	 (activeParallelLoop || get_inside_xomp_nested_parallel(&xomp_status))
       ) {
      // everybody should be co-scheduled -- no need to allow blocking
      qt_global_barrier(me);
    }
    else {
      // in task parallelism -- need to allow blocking
      while (!gb);  // wait for instance of barrier to arrive
      qt_feb_barrier_t * test = (qt_feb_barrier_t *)gb;
      walkSyncTaskList(me); // wait for outstanding tasks to complete
      qt_feb_barrier_enter(me, test);
    }  
}
void XOMP_atomic_start(
    void)
{
    qthread_t *me = qthread_self();

    qthread_lock(me, get_atomic_lock(&xomp_status));
}

void XOMP_atomic_end(
    void)
{
    qthread_t *me = qthread_self();

    qthread_unlock(me, get_atomic_lock(&xomp_status));
}

// needed for full OpenMP 3.0 support
void XOMP_loop_ordered_guided_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  // until ordered is handled defualt to standard loop
  XOMP_loop_guided_init(lower, upper, stride, chunk_size);
}

bool XOMP_loop_ordered_guided_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
  // until ordered is handled defualt to standard loop
  return XOMP_loop_guided_start(a, b, c, d, e, f);
}

bool XOMP_loop_ordered_guided_next(
    long *a,
    long *b)
{
  // until ordered is handled defualt to standard loop
  return XOMP_loop_guided_next(a, b);
}

taskSyncvar_t * qthread_getTaskRetVar(qthread_t * t);
void qthread_setTaskRetVar(qthread_t * t, taskSyncvar_t * v);
syncvar_t *getSyncTaskVar(qthread_t *me);

syncvar_t *getSyncTaskVar(qthread_t *me)
{
  taskSyncvar_t * syncVar = (taskSyncvar_t *)calloc(1,sizeof(taskSyncvar_t));
  qthread_getTaskListLock(me);
  //  qthread_syncvar_empty(me,&(syncVar->retValue));
  syncVar->next_task = qthread_getTaskRetVar(me);
  qthread_setTaskRetVar(me,syncVar);
  qthread_releaseTaskListLock(me);
  
  return &(syncVar->retValue);
}

void qthread_run_needed_task(syncvar_t *value);
void walkSyncTaskList(qthread_t *me)
{
  qthread_getTaskListLock(me);
  taskSyncvar_t * syncVar;
  while ( (syncVar = qthread_getTaskRetVar(me))) {

    // manually check for empty -- check if present in current shepherds ready queue
    //   if present take and start executing
    syncvar_t *lc_p = &syncVar->retValue;

#if AKP_TOUCH_EXECUTION
#if ((QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64) || \
     (QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA64) || \
     (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64) || \
     (QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64))
    {
      // taken from qthread_syncvar_readFF in qthread.c - I want empty not full
      /* I'm being optimistic here; this only works if a basic 64-bit load is
       * atomic (on most platforms it is). Thus, if I've done an atomic read
       * and the syncvar is both unlocked and full, then I figure I can trust
       * that state and do not need to do a locked atomic operation of any
       * kind (e.g. cas) */
      syncvar_t lc_syncVar = *lc_p;
      if (lc_syncVar.u.s.lock == 0 && (lc_syncVar.u.s.state & 2) ) { /* empty and unlocked */
	qthread_releaseTaskListLock(me);
	qthread_run_needed_task(lc_p);
	qthread_getTaskListLock(me);
	syncVar = qthread_getTaskRetVar(me);
      }
    }
#endif
#endif
    qthread_syncvar_readFF(me, NULL, lc_p);
    qthread_setTaskRetVar(me,syncVar->next_task);
    free(syncVar);
  }
  qthread_releaseTaskListLock(me);
  return;
}

aligned_t taskId = 1; // start at first non-master shepherd

void XOMP_task(
    void (*func) (void *),
    void *arg,
    void (*cpyfunc) (void *,
	       void *),
    long arg_size,
    long arg_align,
    bool if_clause,
    unsigned untied)
{
  qthread_t *const me = qthread_self();

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
#else
  aligned_t id = qthread_incr(&taskId,1);
  qthread_debug(LOCK_DETAILS, "me(%p) creating task for shepherd %d\n", me, id%qthread_num_shepherds());
#endif
  syncvar_t *ret = getSyncTaskVar(me); // get new syncvar_t -- setup openmpThreadId (if needed)
  void * arg_copy = NULL;
  if ((sizeof(aligned_t) * 128) < arg_size){
    arg_copy = malloc(arg_size);
    memcpy(arg_copy,arg,arg_size);
  }
  qthread_f qfunc = (qthread_f)func;
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_fork_syncvar_to(qfunc, arg, arg_copy, arg_size, ret, qthread_shep(me));
#else
  qthread_fork_syncvar_to(qfunc, arg, arg_copy, arg_size, ret, id%qthread_num_shepherds());
#endif
}

void XOMP_taskwait(
    void)
{
  qthread_t *const me = qthread_self();
  walkSyncTaskList(me);
}

int staticUpper = 0;
int staticChunkSize = 0;

void XOMP_loop_static_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  qthread_t *const me = qthread_self();
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,me);
#else
  int myid = qthread_shep(me);
#endif
  qqloop_step_handle_t *qqhandle = NULL;

  if (qt_global_arrive_first(myid,xomp_get_nested(&xomp_status))) {
    qqhandle  =  qt_loop_rose_queue_create(lower,
					   upper,
					   stride);
    
    qqhandle->chunkSize = chunk_size;
    qqhandle->type = STATIC_SCHED;
    
    orderedLoopCount = 0;
    
    qqloop_set_value(qqhandle);
  }
  else {
    // wait for value
    qqhandle = qqloop_get_value(myid);
  }

  currentIteration[myid] = 1;
  staticStartCount[myid] = 0;
  staticUpper = upper;
  staticChunkSize = chunk_size;
  return;
}

void XOMP_loop_dynamic_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  XOMP_loop_guided_init(lower, upper, stride, chunk_size);
  /* AKP: until I get the ROSE patch install locally
  //  int i;
  qthread_t *const me = qthread_self();
  qqloop_step_handle_t *qqhandle = NULL;
  
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,me);
#else
  int myid = qthread_shep(me);
#endif
  
  if (qt_global_arrive_first(myid,xomp_get_nested(&xomp_status))) {
    qqhandle  =  qt_loop_rose_queue_create(lower,
					   upper,
					   stride);
    
    qqhandle->chunkSize = chunk_size;
    qqhandle->type = DYNAMIC_SCHED;
    
    qqloop_set_value(qqhandle);
  }
  else {
    // wait for value
    qqhandle = qqloop_get_value(myid);
  }
  
  firstTime[myid] = 1;
  smallBlock[myid] = qqhandle->assignStop - qqhandle->assignNext;
  */
  return;
}


int runtime_sched_chunk = 1;

void XOMP_loop_runtime_init(
    int lower,
    int upper,
    int stride)
{
  //  int i;
  qthread_t *const me = qthread_self();
  qqloop_step_handle_t *qqhandle = NULL;
  
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,me);
#else
  int myid = qthread_shep(me);
#endif

  switch(get_runtime_sched_option(&xomp_status))
    {
      int chunk_size = 1; // For all these something has to be done to correctly compute chunk_size  -BV
    case GUIDED_SCHED: // Initalize guided scheduling at runtime
      XOMP_loop_guided_init(lower, upper, stride, chunk_size);
      break;
    case STATIC_SCHED: // Initalize static scheding at runtime
      XOMP_loop_static_init(lower, upper, stride, chunk_size);
      break;
    case DYNAMIC_SCHED: // Initialize dynamic scheduling at runtime
      XOMP_loop_dynamic_init(lower, upper, stride, chunk_size);
      break;
    default:
      fprintf(stderr, "Weird XOMP_loop_runtime_init case happened that should never happen: %d", qqhandle->type);
      abort();
    }


  firstTime[myid] = 1;
  qqhandle = qqloop_get_value(myid);
  smallBlock[myid] = qqhandle->assignStop - qqhandle->assignNext;
  return;
}

//ordered case
void XOMP_loop_ordered_static_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  XOMP_loop_static_init(lower, upper, stride, chunk_size);
}

void XOMP_loop_ordered_dynamic_init(
    int lower,
    int upper,
    int stride,
    int chunk_size)
{
  XOMP_loop_dynamic_init(lower, upper, stride, chunk_size);
}

void XOMP_loop_ordered_runtime_init(
    int lower,
    int upper,
    int stride)
{
  XOMP_loop_runtime_init(lower, upper, stride);
}


// rest of the functions


// omp ordered directive
void XOMP_ordered_start(
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,qthread_self());
#else
  int myid = qthread_shep(qthread_self());
#endif
  while (orderedLoopCount != currentIteration[myid]){}; // spin until my turn
  
  currentIteration[myid]++;
}

void XOMP_ordered_end(
    void)
{
  orderedLoopCount++;
}

bool XOMP_loop_static_start(
    long startLower,
    long startUpper,
    long stride,
    long chunkSize,
    long *returnLower,
    long *returnUpper)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,qthread_self());
#else
  int myid = qthread_shep(qthread_self());
#endif
  int iterationNum = staticStartCount[myid]++;
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int parallelWidth = qthread_num_workers();
#else
  int parallelWidth = qthread_num_shepherds();
#endif
  *returnLower = (iterationNum * chunkSize * parallelWidth) + (myid*chunkSize); // start + offset
  *returnUpper = (staticUpper - (*returnLower + chunkSize) < 0) ? staticUpper : (*returnLower + chunkSize);
  currentIteration[myid] = *returnLower;

  iterationNum++;

  return ((staticUpper - *returnLower) > 0);
}

bool XOMP_loop_dynamic_start(
    long a,
    long b,
    long c,
    long d,
    long *returnLower,
    long *returnUpper)
{
  // set up so we can call guided and the correct actions happen
  return XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
}

bool XOMP_loop_runtime_start(
    long a,
    long b,
    long c,
    long *returnLower,
    long *returnUpper)
{
  // set up so we can call guided and the correct actions happen
  return XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
}

bool XOMP_loop_ordered_static_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
  return XOMP_loop_static_start(a, b, c, d, e, f);
}

bool XOMP_loop_ordered_dynamic_start(
    long a,
    long b,
    long c,
    long d,
    long *e,
    long *f)
{
  return XOMP_loop_dynamic_start(a, b, c, d, e, f);
}

bool XOMP_loop_ordered_runtime_start(
    long a,
    long b,
    long c,
    long *d,
    long *e)
{


  return XOMP_loop_runtime_start(a, b, c, d, e);
}

// next
bool XOMP_loop_static_next(
    long *a,
    long *b)
{
  return XOMP_loop_static_start(0,0,0,staticChunkSize, a, b);
}

bool XOMP_loop_dynamic_next(
    long *returnLower,
    long *returnUpper)
{
  // set up so we can call guided and the correct actions happen
  return XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
}

bool XOMP_loop_runtime_next(
    long *returnLower,
    long *returnUpper)
{
  // set up so we can call guided and the correct actions happen
  return XOMP_loop_guided_start(-1, -1, -1, -1, returnLower, returnUpper);
}

bool XOMP_loop_ordered_static_next(
    long *a,
    long *b)
{
  return XOMP_loop_static_next(a, b);
}

bool XOMP_loop_ordered_dynamic_next(
    long *a,
    long *b)
{
  return XOMP_loop_dynamic_next(a, b);
}

bool XOMP_loop_ordered_runtime_next(
    long *a,
    long *b)
{
  return XOMP_loop_runtime_next(a, b);
}

//--------------end of  loop functions 

aligned_t XOMP_critical = 0;

void XOMP_critical_start(
    void **data)
{
  // wait on omp critical region to be available
  aligned_t *value = (aligned_t*)*data;
  qthread_t *const me = qthread_self();
  aligned_t v;
  if(value == 0) { // null data passed in
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    v = qthread_worker(NULL,me);
#else
    v = qthread_shep(me);
#endif
  }
  else {
    v = *value;
  }
  qthread_readFE(me, &v, &XOMP_critical);
}

void XOMP_critical_end(
    void **data)
{

  aligned_t *value = (aligned_t*)*data;
  qthread_t *const me = qthread_self();
  aligned_t v;
  if(value == 0) { // null data passed in
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    v = -qthread_worker(NULL,me);
#else
    v = -qthread_shep(me);
#endif
  }
  else {
    v = *value;
  }
  qthread_writeF(me, &XOMP_critical, &v);
}

// really should have a include that defines true and false
// assuming that shepherd 0 is the master -- problem if we get
// to nested parallelism where this may not be true
bool XOMP_master(
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,qthread_self());
#else
  int myid = qthread_shep(qthread_self());
#endif
  if (myid == 0) return 1;
  else return 0;
}

// let the first on to the section do it
bool XOMP_single(
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  int myid = qthread_worker(NULL,qthread_self());
#else
  int myid = qthread_shep(qthread_self());
#endif
  if (qt_global_arrive_first(myid,xomp_get_nested(&xomp_status))){ 
    return 1;
  }
  else{
    return 0;
  }
}


// flush without variable list
void XOMP_flush_all(
    void)
{
  perror("XOMP_flush_all not yet implmented"); 
  exit(1);
}

// omp flush with variable list, flush one by one, given each's start address and size
void XOMP_flush_one(
    char *startAddress,
    int nbyte)
{
  perror("XOMP_flush_one not yet implmented"); 
  exit(1);
}

// Wrappered OMP functions from omp.h
// extern void omp_set_num_threads (int);
void omp_set_num_threads (
    int omp_num_threads_requested)
{
  set_inside_xomp_parallel(&xomp_status, TRUE);

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_t *const me = qthread_self();
  qthread_worker_id_t workerid = qthread_worker(NULL,me);
  qthread_shepherd_id_t num_active = qthread_num_workers();
#else
  qthread_shepherd_id_t num_active = qthread_num_shepherds();
#endif
  qthread_shepherd_id_t i, qt_num_threads_requested;

  qt_num_threads_requested = (qthread_shepherd_id_t) omp_num_threads_requested;

  if ( qt_num_threads_requested > num_active)
    {
      for(i=num_active; i < qt_num_threads_requested; i++)
        {
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
          qthread_enable_worker(i);
#else
          qthread_enable_shepherd(i);
#endif
        }
    }
  else if (qt_num_threads_requested < num_active)
    {
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
      if ((workerid <= num_active) && (workerid>=qt_num_threads_requested))
      	{
	  qt_num_threads_requested--;
	}
#endif
      for(i=num_active-1; i >= qt_num_threads_requested; i--)
        {
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
          if (workerid != i){
	    qthread_disable_worker(i);
	    qthread_pack_workerid(i,-1);
	    //	    if(workerid == i) yield_thread();
	  }
#else
          qthread_disable_shepherd(i);
#endif
        }
    }
  
  if (qt_num_threads_requested != num_active){ 
    // need to reset the barrier size and the first arrival size (if larger or smaller)
    qtar_resize(qt_num_threads_requested);
    qt_barrier_resize(qt_num_threads_requested);
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    qthread_worker_id_t newId = 0;
    for(i=0; i < qt_num_threads_requested; i++) {  // repack id's
      qthread_pack_workerid(i,newId++);
    }
    if (workerid >= qt_num_threads_requested) { // carefull about edge conditions
      qthread_pack_workerid(workerid,newId++);
    }
#endif
  }
}

// extern int omp_get_num_threads (void);
int omp_get_num_threads (
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_worker_id_t num = qthread_num_workers();
#else
  qthread_shepherd_id_t num = qthread_num_shepherds();
#endif
  return (int) num;
}

// extern int omp_get_max_threads (void);
int omp_get_max_threads (
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_worker_id_t num = qthread_num_workers();
#else
  qthread_shepherd_id_t num = qthread_num_shepherds();
#endif
  return (int) num;
}

// extern int omp_get_thread_num (void);
int omp_get_thread_num (
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
  qthread_worker_id_t id = qthread_worker (NULL, NULL);
#else
  qthread_shepherd_id_t id = qthread_shep (NULL);
#endif
  return (int) id;
}

// extern int omp_get_num_procs (void);
int omp_get_num_procs (
    void)
{
  return get_nprocs();
}

// extern int omp_in_parallel (void);
int omp_in_parallel (
    void)
{
  return (int) get_inside_xomp_parallel(&xomp_status);
}

// extern int omp_in_final (void);
char omp_in_final(void)
{
  return 0; // needs to be fixed when omp final clause is supported (coming 3.1)
}

// extern void omp_set_dynamic (int);
void omp_set_dynamic (
    int val)
{
  set_xomp_dynamic(val, &xomp_status);
}

// extern int omp_get_dynamic (void);
int omp_get_dynamic (
    void)
{
  return get_xomp_dynamic(&xomp_status);
}

// extern void omp_init_lock (omp_lock_t *);
void omp_init_lock (
    void *pval)
{
  qthread_t *const me = qthread_self();
  qthread_syncvar_empty(me, pval);
}

// extern void omp_destroy_lock (omp_lock_t *);
void omp_destroy_lock (
    void *pval)
{
}

// extern void omp_set_lock (omp_lock_t *);
void omp_set_lock (
    void *pval)
{
  qthread_t *const me = qthread_self();
  qthread_syncvar_writeEF_const(me, pval, 1);
}

// extern void omp_unset_lock (omp_lock_t *);
void omp_unset_lock (
    void *pval)
{
  qthread_t *const me = qthread_self();
  qthread_syncvar_readFE(me, NULL, pval);
}

// extern int omp_test_lock (omp_lock_t *);
int omp_test_lock (
    void *pval)
{
  perror("qthread wrapper for omp_test_lock not yet implmented");
  exit(1);
  return 0;
}

// extern void omp_init_nest_lock (omp_nest_lock_t *);
void omp_init_nest_lock (
    void *pval)
{
  perror("qthread wrapper for omp_init_nest_lock not yet implmented");
  exit(1);
}

// extern void omp_destroy_nest_lock (omp_nest_lock_t *);
void omp_destroy_nest_lock (
    void *pval)
{
  perror("qthread wrapper for omp_destroy_nest_lock not yet implmented");
  exit(1);
}
void omp_set_nested (int val)
{
  xomp_nest_level_t b = (val)? ALLOW_NEST:NO_NEST;
  xomp_set_nested(&xomp_status, b);
}

int64_t omp_get_nested (void)
{
  bool b = xomp_get_nested(&xomp_status);
  return (b)? 1:0;
}


// extern void omp_set_nest_lock (omp_nest_lock_t *);
void omp_set_nest_lock (
    void *pval)
{
  perror("qthread wrapper for omp_set_nest_lock not yet implmented");
  exit(1);
}

// extern void omp_unset_nest_lock (omp_nest_lock_t *);
void omp_unset_nest_lock (
    void *pval)
{
  perror("qthread wrapper for omp_unset_nest_lock not yet implmented");
  exit(1);
}

// extern int omp_test_nest_lock (omp_nest_lock_t *);
int omp_test_nest_lock (
    void *pval)
{
  perror("qthread wrapper for omp_test_nest_lock not yet implmented");
  exit(1);
  return 0;
}

// extern double omp_get_wtime (void);
double omp_get_wtime (
    void)
{
  return XOMP_get_wtime(&xomp_status);
}

// extern double omp_get_wtick (void);
double omp_get_wtick (
    void)
{
  return XOMP_get_wtick(&xomp_status);
}
