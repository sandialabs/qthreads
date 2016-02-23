#include<stdarg.h>
#include<stddef.h>
#include<setjmp.h>
#include<string.h>
#include<cmocka.h>
#include<qthread/performance.h>
#include<qthread/logging.h>

typedef enum {
  S1,S2,S3,S4,S5,NumStates1
} state1_t;

const char* state1_names[] = {
    "S1","S2","S3","S4","S5"
};

typedef enum {
  S21,S22,S23,S24,S25,NumStates2
} state2_t;

const char* state2_names[] = {
    "S21","S22","S23","S24","S25"
};

typedef enum {
  S31,S32,S33,S34,S35,NumStates3
} state3_t;

const char* state3_names[] = {
    "S31","S32","S33","S34","S35"
};


qtstategroup_t* group1=NULL;
qtstategroup_t* group2=NULL;
qtstategroup_t* group3=NULL;

static int reset_qtperf(void** state){
  qtlog(PERFDBG, "Freeing qtperf data");
  qtperf_free_data();
  group1 = NULL;
  group2 = NULL;
  group3 = NULL;
  return 0;
}

static void test_create_group(void** state){
  group1 = qtperf_create_state_group(NumStates1, state1_names);
  assert_true(group1 != NULL);
  assert_true(qtperf_check_invariants());
}

static void test_create_perfdata(void**state) {
  qtperfdata_t* data1 = NULL;
  qtperfdata_t* data2 = NULL;
  qtperfdata_t* data3 = NULL;
  group1 = qtperf_create_state_group(NumStates1, state1_names);
  assert_true(group1 != NULL);
  group2 = qtperf_create_state_group(NumStates2, state2_names);
  assert_true(group2 != NULL);
  group3 = qtperf_create_state_group(NumStates3, NULL);
  assert_true(group3 != NULL);
  data1 = qtperf_create_perfdata(group1);
  assert_true(data1 != NULL);
  data2 = qtperf_create_perfdata(group2);
  assert_true(data2 != NULL);
  data3 = qtperf_create_perfdata(group3);
  assert_true(data3 != NULL);
  assert_true(qtperf_check_invariants());
  qtperf_free_data();
  assert_true(qtperf_check_invariants() && "after cleanup");
}

size_t spin(size_t amount){
  size_t i=0;
  size_t r=2;
  for(i=0; i<amount; i++){
    size_t j=0;
    for(j=0; j<100; j++){
      r = r * r;
    }
  }
  return r;
}

static void test_state_transitions(void** state){
  qtperfdata_t* data = NULL;
  size_t i=0;
  group1 = qtperf_create_state_group(NumStates1, state1_names);
  assert_true(group1 != NULL);
  data = qtperf_create_perfdata(group1);
  assert_true(data != NULL);
  qtperf_start();
  qtperf_enter_state(data, S1);
  assert_true(data->current_state == S1);
  for(i=0; i<1000; i++){
    spin(100);
    if(i%2 == 0){
      qtperf_enter_state(data, S2);
      spin(1000);
      assert_true(data->current_state == S2);
    }
    if(i%17 == 0){
      qtperf_enter_state(data, S3);
      spin(500);
      assert_true(data->current_state == S3);
    }
    qtperf_enter_state(data, S4);
    assert_true(data->current_state == S4);
    spin(2000);
  }
  qtperf_enter_state(data, S5);
  qtperf_stop();
  qtperf_free_data();
  assert_true(qtperf_check_invariants() && "state transistions done");
}

static void test_check_invariants(void** state) {
  assert_true(qtperf_check_invariants());
}

int main(int argc, char** argv){
  const struct CMUnitTest test[] ={
    cmocka_unit_test(test_check_invariants),
    cmocka_unit_test_teardown(test_create_group,reset_qtperf),
    cmocka_unit_test_teardown(test_create_perfdata,reset_qtperf),
    cmocka_unit_test_teardown(test_state_transitions,reset_qtperf)
  };
  return cmocka_run_group_tests(test,NULL,NULL);
}
