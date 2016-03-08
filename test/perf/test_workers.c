
#include<stdarg.h>
#include<stddef.h>
#include<setjmp.h>
#include<string.h>
#include<cmocka.h>
#include<qthread/qthread.h>
#include<qthread/performance.h>
#include<qthread/logging.h>

aligned_t spin(){
  size_t i=0;
  aligned_t result=2;
  for(i=0; i<1000000; i++){
    result = result * result + i;
  }
  return result;
}

#define TEST 1
size_t num_threads=5;
void test_spinners(void** state){
  size_t i=0;
  aligned_t ret=0;
  qtperf_set_instrument_workers(1);
  qtperf_start();
  qthread_initialize();
  for(i=0; i<num_threads; i++){
    qthread_fork(spin, NULL,&ret);
  }
  for(i=0; i<num_threads; i++){
    qthread_readFE(NULL, &ret);
  }
  qtperf_stop();
  qtlog(TEST, "Printing results...");
  qtperf_print_results();
  qtlog(TEST, "done printing results");
  assert_true(qtperf_check_invariants());
}

// Test the spin_lock function to make sure it prevents simultaneous
// edits to a data structure. this function is copied from
// performance.c in order to test it as written. It's lame, but
// simple.
static inline void spin_lock(volatile uint32_t* busy);

static inline void spin_lock(volatile uint32_t* busy){
  register bool stopped = 0;
  while(qthread_cas32(busy, 0, 1) != 0){
    stopped = 1;
  }
  if(stopped){
    qtlog(LOGWARN, "Stopped in spinlock");
  }
}
void struct_edit(int* strct, volatile uint32_t* busy){
  int start =0;
  size_t i=0;
  for(i=0; i<100; i++){
    spin_lock(busy);
    start = *strct;
    for(i=0; i<1000000; i++){
      *strct = *strct+1;
    }
    assert_true(*strct == start+1000000);
    *busy = 0;
  }
}

static void test_spinlock(void** state) {
  volatile uint32_t busy=0;
  aligned_t ret=0;
  size_t i=0;
  qtperf_start();
  qthread_initialize();
  for(i=0; i<10; i++){
    qthread_fork(struct_edit, NULL, &ret);
  }
  for(i=0; i<10; i++){
    qthread_readFE(NULL,&ret);
  }
  qtperf_stop();
  assert_true(qtperf_check_invariants());
}

void test_teardown(void** state){
  qtperf_free_data();
  assert_true(qtperf_check_invariants());
}


int main(int argc, char** argv){
  const struct CMUnitTest test[] ={
    cmocka_unit_test(test_spinners),
    cmocka_unit_test(test_spinlock),
    cmocka_unit_test(test_teardown)
  };
  return cmocka_run_group_tests(test,NULL,NULL);
}
