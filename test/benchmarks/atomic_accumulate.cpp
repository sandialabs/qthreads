#include "atomic_bench.hpp"

// Similar to atomic reduction operations (See C++ P3111)
std::size_t increment(std::size_t val,
                      std::atomic<std::size_t> &entry) noexcept {
  entry.fetch_add(val, std::memory_order_relaxed);
  return 1uz;
}

std::size_t increment_na(std::size_t val,
                         std::size_t volatile &entry) noexcept {
  entry += val;
  return 1uz;
}

int main() {
  atomic_bench<0uz, increment>();
  atomic_bench_baseline<0uz, increment_na>();
  return 0;
}
