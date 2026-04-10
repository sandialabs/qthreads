#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include <qthread/qtimer.h>

#include "argparsing.h"

// Only for functions.
// Call a function but force it not to be inlined.
template <typename F, typename... As>
auto invoke_noinline(F *volatile f,
                     As... as) noexcept(noexcept(f(std::forward<As>(as)...))) {
  return f(std::forward<As>(as)...);
}

template <typename T>
struct nargs_empty;

template <std::size_t... i>
struct nargs_empty<std::index_sequence<i...>> {
  // Use inside a pack expansion to just repeat t for each entry in the pack.
  template <typename t, auto>
  using repeat_for_pack = t;

  // Empty function with
  static void empty(repeat_for_pack<std::size_t, i>...) noexcept {}

  // Return first argument.
  // Again, for use in a pack expansion for repeating a value.
  template <typename T>
  static auto first(T a, T b) noexcept {
    return a;
  }

  static void call_empty(std::size_t j) noexcept {
    // Force the call through a volatile pointer to prevent inlining.
    invoke_noinline(&empty, first(j, i)...);
  }
};

template <std::size_t nargs>
void bench(std::size_t num_reps, qtimer_t timer) noexcept {
  qtimer_start(timer);
  for (uint64_t i = 0u; i < num_reps; i++) {
    nargs_empty<std::make_index_sequence<nargs>>::call_empty(i);
  }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf(
    "Time for %lu function calls with %lu uint64_t arguments is: %f seconds\n",
    num_reps,
    nargs,
    time);
}

template <std::size_t... sizes>
void bench_at_sizes(std::size_t num_reps, qtimer_t timer) noexcept {
  (bench<sizes>(num_reps, timer), ...);
}

template <typename T>
void bench_at_sizes_from_sequence(std::size_t num_reps, qtimer_t timer);

template <std::size_t... sizes>
void bench_at_sizes_from_sequence(std::index_sequence<sizes...>,
                                  std::size_t num_reps,
                                  qtimer_t timer) noexcept {
  bench_at_sizes<sizes...>(num_reps, timer);
}

template <std::size_t, std::size_t, typename>
struct index_range_impl;

template <std::size_t start, std::size_t step, std::size_t... entries>
struct index_range_impl<start, step, std::index_sequence<entries...>> {
  using impl = std::index_sequence<(start + step * entries)...>;
};

template <std::size_t start, std::size_t stop, std::size_t step>
using index_range = typename index_range_impl<
  start,
  step,
  std::make_index_sequence<(start <= stop ? (stop - start) / step : 0)>>::impl;

template <std::size_t start, std::size_t stop, std::size_t step>
void bench_at_size_range(std::size_t num_reps, qtimer_t timer) noexcept {
  bench_at_sizes_from_sequence(
    index_range<start, stop, step>(), num_reps, timer);
}

int main(int argc, char *argv[]) {
  uint64_t num_reps = 1000000ull;
  NUMARG(num_reps, "NUM_REPS");
  qtimer_t timer = qtimer_create();
  bench_at_size_range<2, 66, 2>(num_reps, timer);

  qtimer_destroy(timer);
}
