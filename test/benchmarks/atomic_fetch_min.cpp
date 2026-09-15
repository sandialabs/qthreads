#include "atomic_bench.hpp"

// fetch_min is similar to the atomic update done in BFS and SSSP
#ifdef __cpp_lib_atomic_min_max
std::size_t set_earliest(std::size_t val,
                         std::atomic<std::size_t> &entry) noexcept {
  entry.fetch_min(val, std::memory_order_relaxed);
  return val + 1uz;
}
#else
std::size_t set_earliest(std::size_t val,
                         std::atomic<std::size_t> &entry) noexcept {
  std::size_t old_entry = entry.load(std::memory_order_relaxed);
  while (old_entry > val && !entry.compare_exchange_weak(
                              old_entry, val, std::memory_order_relaxed));
  return val + 1uz;
}
#endif

std::size_t set_earliest_na(std::size_t val,
                            std::size_t volatile &entry) noexcept {
  if (val < entry) entry = val;
  return val + 1uz;
}

int main() {
  atomic_bench<std::numeric_limits<std::size_t>::max(), set_earliest>();
  atomic_bench_baseline<std::numeric_limits<std::size_t>::max(),
                        set_earliest_na>();
  return 0;
}
