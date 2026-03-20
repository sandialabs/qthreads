#include <stdint.h>
#include <stdlib.h>

#include "argparsing.h"

extern double qt_ctx_swap_bench(uint64_t num_swaps);

int main(int argc, char *argv[]) {
  uint64_t num_swaps = 1000000ull;
  NUMARG(num_swaps, "NUM_SWAPS");
  if (num_swaps % 2ull) {
    iprintf("Error: number of context swaps must be even.\n");
    abort();
  }
  double time = qt_ctx_swap_bench(num_swaps);
  printf("Time for %lu context swaps is: %f seconds.\n", num_swaps, time);
}
