#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <qthread/qtimer.h>

#include "qt_atomic_wait.h"

#include "argparsing.h"

qt_atomic_wait_t flag{qt_atomic_wait_empty};

void thread_func(qt_atomic_wait_t *flag_ptr, uint64_t num_reps) noexcept {
  for (uint64_t i = 0u; i < num_reps / 2u; i++) {
    do {
      qt_wait_on_address(flag_ptr, qt_atomic_wait_empty);
    } while (qt_atomic_wait_load(flag_ptr) == qt_atomic_wait_empty);
    qt_atomic_wait_set_empty(flag_ptr);
    qt_wake_all(flag_ptr);
  }
}

int main(int argc, char *argv[]) {
  uint64_t num_reps = 1000ull;
  NUMARG(num_reps, "NUM_REPS");

  std::thread t{thread_func, &flag, num_reps};

  qtimer_t timer = qtimer_create();
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps / 2u; i++) {
    qt_atomic_wait_set_full(&flag);
    qt_wake_all(&flag);
    do {
      qt_wait_on_address(&flag, qt_atomic_wait_full);
    } while (qt_atomic_wait_load(&flag) == qt_atomic_wait_full);
  }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  qtimer_destroy(timer);

  t.join();
  printf(
    "Time for %lu thread futex wait cycles is: %f seconds.\n", num_reps, time);
}
