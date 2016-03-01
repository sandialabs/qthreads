
#include<stdarg.h>
#include<stddef.h>
#include<setjmp.h>
#include<string.h>
#include<cmocka.h>
#include<qthread/performance.h>
#include<qthread/logging.h>

void test_setup(void** state){
  //qtperf_set_instrument_workers(1);
  qthread_initialize();
  assert_true(qtperf_check_invariants());
}

void test_teardown(void** state){
  //qtperf_free_data();
  assert_true(qtperf_check_invariants());
}

int main(int argc, char** argv){
  const struct CMUnitTest test[] ={
    cmocka_unit_test(test_setup),
    cmocka_unit_test(test_teardown)
  };
  return cmocka_run_group_tests(test,NULL,NULL);
}
