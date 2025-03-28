#ifndef QT_THREADPOOL_H
#define QT_THREADPOOL_H

#include <stdint.h>

#include <qthread/common.h>

typedef int (*qt_threadpool_func_type)(void *);

typedef enum {
  POOL_INIT_SUCCESS,
  POOL_INIT_ALREADY_INITIALIZED,
  POOL_INIT_NO_THREADS_SPECIFIED,
  POOL_INIT_OUT_OF_MEMORY,
  // TODO: better granularity when forwarding errors from thread creation.
  POOL_INIT_ERROR
} pool_init_status;

pool_init_status pool_init(uint32_t num_threads);
void pool_destroy();
void pool_run_on_all(qt_threadpool_func_type func, void *arg);

#endif
