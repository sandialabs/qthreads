#include "atomic_bench.hpp"

// Similar to the dependency computation idiom in fine-grained DAG execution.
// Also similar to atomic reference counting synchronization.
std::size_t decrement_and_reset(std::size_t val,
                                std::atomic<std::size_t> &entry) noexcept {
  if (!entry.fetch_sub(val, std::memory_order_relaxed)) entry.fetch_add(5uz);
  return 1uz;
}

std::size_t decrement_and_reset_na(std::size_t val,
                                   std::size_t volatile &entry) noexcept {
  if (!(entry -= val)) entry = 5uz;
  return 1uz;
}

int main() {
  atomic_bench<5uz, decrement_and_reset>();
  atomic_bench_baseline<5uz, decrement_and_reset_na>();
  return 0;
}
