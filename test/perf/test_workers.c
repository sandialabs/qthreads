
#include<stdarg.h>
#include<stddef.h>
#include<setjmp.h>
#include<string.h>
#include<cmocka.h>
#include<qthread/performance.h>
#include<qthread/logging.h>

int setup(void** state){
  return 0;
}

int teardown(void**state){
  return 0;
}

void test_setup(void** state){
}

int main(int argc, char** argv){
  const struct CMUnitTest test[] ={
    cmocka_unit_test_setup_teardown(test_setup, setup, teardown)
  };
  return cmocka_run_group_tests(test,NULL,NULL);
}
