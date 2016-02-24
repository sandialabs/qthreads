#ifndef PERFORMANCE_INLINE_H
#define PERFORMANCE_INLINE_H
#include "qt_shepherd_innards.h"

// This function makes a state transition for the given worker thread.
static inline void worker_transition(struct qthread_worker_s* worker, worker_state_t newstate){
#ifdef QTHREAD_PERFORMANCE
  if(worker->performance_data == NULL){
    qtlogargs(LOGWARN, "Worker thread %d was not initialized for data collection, ensure qtperf_set_instrument_workers is called before qthread_initialize()", worker->worker_id);
    return;
  }
  if(_collect_workers){
    qtperf_enter_state(worker->performance_data, newstate);
  }
#endif
}

#endif
