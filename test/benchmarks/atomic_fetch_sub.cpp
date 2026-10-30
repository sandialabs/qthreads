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
  // Have to cast back to non-volatile to make the compiler happy.
  // It's volatile in the signature just to force a cold read from memory.
  // In this case, we have to use a fence instead.
  __asm__ __volatile__("" ::: "memory");
  std::size_t &entry_nv = const_cast<std::size_t &>(entry);
  if (!(entry_nv -= val)) entry = 5uz;
  return 1uz;
}

int main() {
  atomic_bench<5uz, decrement_and_reset>();
  atomic_bench_baseline<5uz, decrement_and_reset_na>();
  return 0;
}
