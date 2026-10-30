#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include <qthread/qtimer.h>

#include "argparsing.h"

// Design: at each iteration call into an entry in an array
// of volatile function pointers. The volatile prevents
// inlining. Get the next index to use based on some cheap
// computations inside each function called.
// The hope is that that it's enough to confuse the branch predictor.
// Get the next index to use based on the current index of
// iteration and the previous
// The compiler's strength reduction pass should make the
// mod operation very cheap, though it's probably a good idea
// to only mod by powers of two just to be safe.

template <std::size_t offset, std::size_t mod>
uint64_t add_n(std::size_t i, std::size_t current) noexcept {
  return (current + i + offset) % mod;
}

using fptr_t = decltype(&add_n<0, 2>);

template <std::size_t... indices>
auto gen_array_impl(std::index_sequence<indices...>) noexcept {
  return std::array<fptr_t volatile, sizeof...(indices)>{
    add_n<indices, sizeof...(indices)>...};
}

template <std::size_t size>
auto gen_array() noexcept {
  return gen_array_impl(std::make_index_sequence<size>());
}

void baseline(std::size_t num_reps, qtimer_t timer) noexcept {
  std::size_t current = 0;
  fptr_t volatile fptr = &add_n<1, 4>;
  qtimer_start(timer);
  for (std::size_t i = 0; i < num_reps; i++) { current = fptr(i, current); }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf(
    "Baseline time for %zu function calls is: %f seconds.\n", num_reps, time);
}

template <std::size_t fptr_array_size>
void bench(std::size_t num_reps, qtimer_t timer) noexcept {
  static_assert(fptr_array_size > 0ull,
                "Need nonzero array size. There has to be something to call.");
  std::size_t current = 0;
  auto fptr_array = gen_array<fptr_array_size>();
  qtimer_start(timer);
  for (std::size_t i = 0; i < num_reps; i++) {
    current = fptr_array[current](i, current);
  }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf("Time for %zu function calls (with array size %zu) without branch "
         "predicts is: %f seconds.\n",
         num_reps,
         fptr_array_size,
         time);
}

template <std::size_t... sizes>
void bench_at_sizes(std::size_t num_reps, qtimer_t timer) noexcept {
  (bench<sizes>(num_reps, timer), ...);
}

int main(int argc, char *argv[]) {
  std::size_t num_reps = 1000000ull;
  NUMARG(num_reps, "NUM_REPS");
  qtimer_t timer = qtimer_create();

  baseline(num_reps, timer);
  bench_at_sizes<2, 4, 8, 16, 32, 64, 128, 256, 512, 1024>(num_reps, timer);

  qtimer_destroy(timer);
}
