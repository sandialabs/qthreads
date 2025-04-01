#include "argparsing.h"
#include "qt_threadpool.h"

static int on_thread_test(void *arg) {
  test_check(get_num_delegated_threads() == 1);
  printf("hello from thread\n");
  return 0;
}

int main() {
  test_check(get_num_delegated_threads() == 1);
  hw_pool_init(2);
  test_check(get_num_delegated_threads() == 2);
  hw_pool_destroy();
  test_check(get_num_delegated_threads() == 1);
  hw_pool_init(2);
  run_on_current_pool(&on_thread_test, NULL);
  hw_pool_destroy();
  printf("exited successfully\n");
  fflush(stdout);
  return 0;
}
