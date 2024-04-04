#include <cmocka.h>
#include <qthread/logging.h>
#include <qthread/performance.h>
#include <qthread/qthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

typedef enum {
  FEB_WORKING,
  FEB_WAITING,
  FEB_COMPLETE,
  FEB_NUM_STATES
} febstate_t;

char const *feb_names[] = {
  "FEB_WORKING", "FEB_WAITING", "FEB_COMPLETE", "FEB_NUM_STATES"};
aligned_t available = 0;
int strct = 0;
aligned_t bit = 0;
#define SPIN 1
#define NITER 100000

aligned_t struct_edit(void *data) {
  size_t i = 0;
  aligned_t ret = 0;
  qtperfdata_t *mydata = (qtperfdata_t *)data;
  qtperf_enter_state(mydata, FEB_WORKING);
  for (i = 0; i < 100; i++) {
    size_t j = 0;
    qtperf_enter_state(mydata, FEB_WAITING);
    qthread_readFE(NULL, &available);
    qtperf_enter_state(mydata, FEB_WORKING);
    for (j = 0; j < NITER; j++) { strct = strct * (strct + 1); }
    qthread_writeEF(&available, &ret);
  }
  qtperf_enter_state(mydata, QTPERF_INVALID_STATE);
  ret = strct;
  return ret;
}

typedef enum { RUNNING, DONE, TOTAL_NUM_STATES } total_t;

char const *total_names[] = {"RUNNING", "DONE"};

static void test_feblock(void **state) {
  aligned_t ret = 0;
  size_t i = 0;
  qtstategroup_t *spingroup = NULL;
  qtstategroup_t *totalgroup = NULL;
  qtperfdata_t *totaldata = NULL;
  qtperf_start();
  qtperf_set_instrument_qthreads(1);
  qthread_initialize();
  qthread_fill(&available);
  qthread_empty(&ret);
  spingroup =
    qtperf_create_state_group(FEB_NUM_STATES, "Spin Testing", feb_names);
  totalgroup =
    qtperf_create_state_group(TOTAL_NUM_STATES, "Total Time", total_names);
  totaldata = qtperf_create_perfdata(totalgroup);
  qtperf_enter_state(totaldata, RUNNING);
  for (i = 0; i < 10; i++) {
    qtperfdata_t *spindata = qtperf_create_perfdata(spingroup);
    qthread_fork(struct_edit, (void *)spindata, &ret);
  }
  for (i = 0; i < 10; i++) { qthread_readFE(NULL, &ret); }
  qtperf_enter_state(totaldata, DONE);
  qtperf_stop();
  assert_true(qtperf_check_invariants());
  qtperf_print_delimited(qtperf_qthreads_group, ",", 1, "= ");
  qtperf_print_delimited(spingroup, ",", 1, "+ ");
  qtperf_free_data();
  assert_true(qtperf_check_invariants());
}

int main(int argc, char **argv) {
  const struct CMUnitTest test[] = {cmocka_unit_test(test_feblock)};
  return cmocka_run_group_tests(test, NULL, NULL);
}
