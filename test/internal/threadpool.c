#include "argparsing.h"
#include "qt_threadpool.h"

static int on_thread_test(void *arg) {
  printf("hello from thread\n");
  return 0;
}

int main() {
  pool_init(2);
  pool_destroy();
  pool_init(2);
  pool_run_on_all(&on_thread_test, NULL);
  pool_destroy();
  printf("exited successfully\n");
  fflush(stdout);
  return 0;
}
