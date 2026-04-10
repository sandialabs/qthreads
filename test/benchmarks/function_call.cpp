#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <qthread/qtimer.h>

#include "argparsing.h"

// on arm64, the first 4 arguments are passed as registers
static void four(uint64_t, uint64_t, uint64_t, uint64_t) {}

// On x86, the first 6 arguments are passed as registers
static void six(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {}

// On risc-v the first 8 arguments are passed as registers
static void eight(uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t) {}

// Some intermediate numbers.
static void ten(uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t,
                uint64_t) {}

static void twelve(uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t) {}

static void fourteen(uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t) {}

static void sixteen(uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t,
                    uint64_t) {}

static void eighteen(uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t,
                     uint64_t) {}

static void twenty(uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t) {}

static void twenty_two(uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t) {}

static void twenty_four(uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t) {}

static void twenty_six(uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t) {}

static void twenty_eight(uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t) {}

static void thirty(uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t,
                   uint64_t) {}

static void thirty_two(uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t) {}

static void thirty_four(uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t) {}

static void thirty_six(uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t) {}

static void thirty_eight(uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t,
                         uint64_t) {}

static void forty(uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t,
                  uint64_t) {}

static void forty_two(uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t) {}

static void forty_four(uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t,
                       uint64_t) {}

static void forty_six(uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t,
                      uint64_t) {}

static void forty_eight(uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t,
                        uint64_t) {}

int main(int argc, char *argv[]) {
  uint64_t num_reps = 1000000ull;
  NUMARG(num_reps, "NUM_REPS");
  qtimer_t timer = qtimer_create();
  double time;

  // Route each call through a volatile pointer to prevent inlining.
  // Still allow the branch predictor to do its thing here.
  // Function calls can incur branch mispredicts as well as
  // disruptions to the compiler's register allocation.
  // This measurement doesn't include those knock-on effects.
  auto const volatile four_p = &four;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    // Still allow the branch predictor to do its thing here.
    four_p(i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 4 uint64_t arguments is: %f seconds.\n",
    num_reps,
    time);

  auto const volatile six_p = &six;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) { six_p(i, i, i, i, i, i); }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 6 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile eight_p = &eight;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) { eight_p(i, i, i, i, i, i, i, i); }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 8 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile ten_p = &ten;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    ten_p(i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 10 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twelve_p = &twelve;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twelve_p(i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 12 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile fourteen_p = &fourteen;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    fourteen_p(i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 14 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile sixteen_p = &sixteen;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    sixteen_p(i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 16 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile eighteen_p = &eighteen;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    eighteen_p(i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 18 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twenty_p = &twenty;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twenty_p(i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 20 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twenty_two_p = &twenty_two;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twenty_two_p(
      i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 22 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twenty_four_p = &twenty_four;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twenty_four_p(
      i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 24 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twenty_six_p = &twenty_six;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twenty_six_p(i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 26 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile twenty_eight_p = &twenty_eight;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    twenty_eight_p(i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 28 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile thirty_p = &thirty;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    thirty_p(i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i,
             i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 30 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile thirty_two_p = &thirty_two;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    thirty_two_p(i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 32 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile thirty_four_p = &thirty_four;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    thirty_four_p(i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 34 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile thirty_six_p = &thirty_six;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    thirty_six_p(i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 36 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile thirty_eight_p = &thirty_eight;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    thirty_eight_p(i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i,
                   i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 38 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile forty_p = &forty;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    forty_p(i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i,
            i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 40 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile forty_two_p = &forty_two;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    forty_two_p(i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 42 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile forty_four_p = &forty_four;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    forty_four_p(i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i,
                 i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 44 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile forty_six_p = &forty_six;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    forty_six_p(i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i,
                i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 46 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  auto const volatile forty_eight_p = &forty_eight;
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    forty_eight_p(i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i,
                  i);
  }
  qtimer_stop(timer);
  time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with 48 uint64_t arguments is: %f seconds\n",
    num_reps,
    time);

  qtimer_destroy(timer);
}
