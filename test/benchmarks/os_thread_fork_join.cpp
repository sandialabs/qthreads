#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <qthread/qtimer.h>

#include "argparsing.h"

int main(int argc, char *argv[]) {
  uint64_t num_reps = 1000ull;
  NUMARG(num_reps, "NUM_REPS");

  qtimer_t timer = qtimer_create();
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    std::thread t{[]() noexcept {}};
    t.join();
  }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  qtimer_destroy(timer);

  printf(
    "Time for %lu thread launch/join cycles is: %f seconds.\n", num_reps, time);
}
