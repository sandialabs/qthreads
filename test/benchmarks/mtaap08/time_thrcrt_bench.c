#include "argparsing.h"
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#define NUM_THREADS 1000000
#define MAX_THREADS 400
#define THREAD_BLOCK 200

static aligned_t qincr(void *arg) { return 0; }

int main(int argc, char *argv[]) {
  aligned_t rets[MAX_THREADS];
  qtimer_t timer = qtimer_create();
  double cumulative_time = 0.0;
  size_t counter = 0;

  if (qthread_initialize() != QTHREAD_SUCCESS) {
    fprintf(stderr, "qthread library could not be initialized!\n");
    exit(EXIT_FAILURE);
  }
  CHECK_VERBOSE();

  for (int iteration = 0; iteration < 10; iteration++) {
    qtimer_start(timer);
    for (int i = 0; i < MAX_THREADS; i++) {
      qthread_fork(qincr, NULL, &(rets[i]));
    }
    counter = MAX_THREADS;
    while (counter < NUM_THREADS) {
      for (int i = 0; i < THREAD_BLOCK; i++) {
        qthread_readFF(NULL, &(rets[i]));
      }
      for (int i = 0; i < THREAD_BLOCK; i++) {
        qthread_fork(qincr, &counter, &(rets[i]));
      }
      for (int i = THREAD_BLOCK; i < MAX_THREADS; i++) {
        qthread_readFF(NULL, &(rets[i]));
      }
      for (int i = THREAD_BLOCK; i < MAX_THREADS; i++) {
        qthread_fork(qincr, &counter, &(rets[i]));
      }
      counter += MAX_THREADS;
    }
    for (int i = 0; i < MAX_THREADS; i++) { qthread_readFF(NULL, &(rets[i])); }
    qtimer_stop(timer);
    iprintf("\ttest iteration %i: %f secs\n", iteration, qtimer_secs(timer));
    cumulative_time += qtimer_secs(timer);
  }
  printf("qthread time: %f\n", cumulative_time / 10.0);

  return 0;
}

/* vim:set expandtab */
