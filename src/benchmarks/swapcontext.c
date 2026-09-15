#include <stdint.h>
#include <stdlib.h>

#include <qthread/qtimer.h>

#include "qt_context.h"
#include "qt_visibility.h"

#ifdef USE_SYSTEM_SWAPCONTEXT
#define QT_MAKECONTEXT makecontext
#define QT_GETCONTEXT getcontext
#define QT_SWAPCONTEXT swapcontext
#else
#define QT_MAKECONTEXT qt_makectxt
#define QT_GETCONTEXT getcontext
#define QT_SWAPCONTEXT qt_swapctxt
#endif

#ifdef QTHREAD_MAKECONTEXT_SPLIT
#error                                                                         \
  "Context swapping benchmark does not currently support split makecontext."
#endif

#define SWAP_BENCH_STACK_SIZE 32768u

typedef struct {
  qt_context_t inner;
  qt_context_t outer;
} context_pair;

// This never returns, it just immediately swaps back to
// the outer context (passed as an argument) whenever entered.
static void *ctx_swap_inner(void *arg) {
  context_pair *contexts = arg;
  while (1) {
    // Swap back to the outer one
    QT_SWAPCONTEXT(&contexts->inner, &contexts->outer);
  }
}

API_FUNC double qt_ctx_swap_bench(uint64_t num_swaps) {
  context_pair contexts;
  // Save current context
  QT_GETCONTEXT(&contexts.outer);
  // Make a context to switch to, running ctx_swap_inner.
  // Initialization like this is required by the makecontext API.
  // Weird, but okay.
  QT_GETCONTEXT(&contexts.inner);
  // TODO: can we get away with just using alloca here instead?
  contexts.inner.uc_stack.ss_sp = malloc(SWAP_BENCH_STACK_SIZE);
  contexts.inner.uc_stack.ss_size = SWAP_BENCH_STACK_SIZE;
  QT_MAKECONTEXT(
    &contexts.inner, (void (*)(void))&ctx_swap_inner, 1, &contexts);
  // Start timer
  qtimer_t timer = qtimer_create();
  qtimer_start(timer);
  // Actual benchmark.
  // Swap into and out of the inner context repeatedly
  // without executing any other work.
  // This is all on the same thread and everything is small
  // enough to at least keep everything in the l1 cace on
  // nearly any hardware these days, so this should allow
  // us to get a decent estimate of the isolated cost of
  // a context swap.
  for (uint64_t i = 0u; i < num_swaps / 2; i++) {
    // Swap to the inner context;
    QT_SWAPCONTEXT(&contexts.outer, &contexts.inner);
  }
  qtimer_stop(timer);
  double time_elapsed = qtimer_secs(timer);
  qtimer_destroy(timer);
  free(contexts.inner.uc_stack.ss_sp);
  return time_elapsed;
}
