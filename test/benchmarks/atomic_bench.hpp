#ifndef QT_ATOMIC_BENCH_H
#define QT_ATOMIC_BENCH_H

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>

#include <qthread/common.h>
#include <qthread/qtimer.h>

#include <qt_atomics.h>

#include "argparsing.h"

// Max page size on currently known systems.
// Used for forcing distinct pages to be allocated
// when doing the non-atomic baseline measurement.
static constexpr std::size_t max_known_page_size = 64uz * 1024uz;

// Naive implementation to check if two numbers are coprime
// using the Euclidean algorithm.
bool are_coprime(std::size_t a, std::size_t b) noexcept {
  if (a < b) return are_coprime(b, a);
  std::size_t t;
  while (b) {
    t = b;
    b = a % b;
    a = t;
  }
  return a == 1uz;
}

// Multiplication by a non-divisor is an easy way to get
// a bijection from the ring of integers mod something to itself.
// This generates "num_requested" bijections on the ring of
// integers mod "base" and stores them in "arr".
// The bijections are not necessarily unique.
void ncoprimes(std::size_t *arr,
               std::size_t num_requested,
               std::size_t base) noexcept {
  if (!base) std::abort();
  if (base < 3uz) {
    for (std::size_t i = 0uz; i < num_requested; i++) { arr[i] = 1uz; }
    return;
  }
  std::size_t num_done = 0uz;
  std::size_t current = 2uz;
  while (num_done < num_requested) {
    if (are_coprime(base, current)) arr[num_done++] = current++;
    // When simulating high-contention situations, base may be small enough
    // that there just aren't enough unique requested coprimes.
    // In that case, just let the available coprimes repeate until
    // the array is full.
    if (current++ == base) current = 2uz;
  }
}

using atomic_reduction_t = std::size_t (*)(std::size_t,
                                           std::atomic_size_t &) noexcept;

template <auto red>
void stress_atomics(std::size_t map_id,
                    std::size_t val,
                    std::size_t index,
                    std::atomic<std::size_t> *buffer,
                    std::size_t num_items,
                    std::size_t num_reps) noexcept {
  for (std::size_t i = 0uz; i < num_reps; i++) {
    val = red(val, buffer[index]);
    index += map_id;
    index %= num_items;
  }
}

static std::atomic<std::size_t> ready{0z};
static std::atomic<std::size_t> finished{0z};

template <auto red>
void on_thread(std::size_t map_id,
               std::size_t val,
               std::size_t index,
               std::atomic<std::size_t> *buffer,
               std::size_t num_items,
               std::size_t num_reps) noexcept {
  ready.fetch_add(1uz, std::memory_order_relaxed);
  while (ready.load(std::memory_order_relaxed)) SPINLOCK_BODY();
  stress_atomics<red>(map_id, val, index, buffer, num_items, num_reps);
  finished.fetch_add(1uz, memory_order_relaxed);
}

auto get_args() noexcept {
  std::array<std::size_t, 3uz> params{4uz, 100000000uz, 1000000000uz};
  NUMARG(params[0uz], "NUM_THREADS");
  NUMARG(params[1uz], "NUM_ITEMS");
  NUMARG(params[2uz], "NUM_REPS");
  if (!params[1uz] || !params[0uz]) std::abort();
  return params;
}

template <size_t init, atomic_reduction_t update>
void atomic_bench() noexcept {
  auto [num_threads, num_items, num_reps] = get_args();
  // Check for ill-defined zero cases
  std::size_t num_reps_per_thread = num_reps / num_threads;
  std::size_t remainder = num_reps % num_threads;
  std::size_t map_ids[num_threads];
  ncoprimes(map_ids, num_threads, num_items);
  std::size_t max_map_id = *std::max_element(map_ids, map_ids + num_threads);
  std::atomic<std::size_t> *items =
    reinterpret_cast<std::atomic<std::size_t> *>(
      malloc(sizeof(std::atomic<std::size_t>) * num_items));
  for (std::size_t i = 0uz; i < num_items; i++) {
    new (&items[i]) std::atomic<std::size_t>(init);
  }
  std::thread pool[num_threads - 1uz];
  // reserve the 0th iteration for the main thread.
  std::size_t first_thread_items =
    num_reps_per_thread + (remainder ? 1uz : 0uz);
  for (std::size_t i = 0uz; i < num_threads - 1uz; i++) {
    pool[i] = std::thread(on_thread<update>,
                          map_ids[i + 1uz],
                          0uz,
                          ((i + 1uz) * num_items) / num_threads,
                          items,
                          num_items,
                          (i + 1uz) < remainder ? num_reps_per_thread + 1uz
                                                : num_reps_per_thread);
  }
  qtimer_t timer = qtimer_create();
  while (ready.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();
  qtimer_start(timer);
  ready.store(0uz, std::memory_order_relaxed);
  stress_atomics<update>(
    map_ids[0uz], 0uz, 0uz, items, num_items, first_thread_items);
  while (finished.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();
  finished.store(0uz, std::memory_order_relaxed);

  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf("Time for running %zu atomic updates to %zu distinct items with %zu "
         "threads: %f seconds\n",
         num_reps,
         num_items,
         num_threads,
         time);
  for (uint64_t i = 0uz; i < num_threads - 1uz; i++) pool[i].join();
  for (uint64_t i = 0uz; i < num_items; i++) items[i].~atomic<std::size_t>();
  free(items);
}

// volatile prevents the compiler from trying to be extra smart and eliminate
// the benchmark loop entirely.
using non_atomic_reduction_t = std::size_t (*)(std::size_t,
                                               std::size_t volatile &);

template <auto red>
void stress_non_atomics(std::size_t map_id,
                        std::size_t val,
                        std::size_t index,
                        std::size_t *buffer,
                        std::size_t num_items,
                        std::size_t num_reps) noexcept {
  for (std::size_t i = 0uz; i < num_reps; i++) {
    val = red(val, buffer[index]);
    index += map_id;
    index %= num_items;
  }
}

template <std::size_t init, auto red>
void on_thread_baseline(std::size_t map_id,
                        std::size_t val,
                        std::size_t index,
                        std::size_t *buffer,
                        std::size_t num_items,
                        std::size_t num_reps) noexcept {
  // Do initialization of the separate buffers here.
  // First-touch NUMA policy will make it so that each thread
  // should get pages in its own NUMA domain.
  for (std::size_t i = 0uz; i < num_items; i++) buffer[i] = init;
  ready.fetch_add(1uz, std::memory_order_relaxed);
  while (ready.load(std::memory_order_relaxed)) SPINLOCK_BODY();
  stress_non_atomics<red>(map_id, val, index, buffer, num_items, num_reps);
  finished.fetch_sub(1uz, std::memory_order_relaxed);
}

// A version for doing non-atomic writes to completely disjoint blocks of memory
// in order to measure how much overhead comes from the synchronization.
template <size_t init, non_atomic_reduction_t update>
void atomic_bench_baseline() noexcept {
  auto [num_threads, num_items, num_reps] = get_args();
  std::size_t num_reps_per_thread = num_reps / num_threads;
  std::size_t remainder = num_reps % num_threads;
  std::size_t num_items_per_thread =
    num_items < num_threads ? 1uz : num_items / num_threads;
  // Still replicate the behavior of having different threads jump around at
  // different intervals. Just have them do it each within their own distinct
  // allocation.
  std::size_t map_ids[num_threads];
  ncoprimes(map_ids, num_threads, num_items_per_thread);
  std::size_t *items[num_threads];
  // Make sure the allocation is big enough to force each one to have a distinct
  // page.
  for (std::size_t i = 0uz; i < num_threads; i++)
    items[i] = reinterpret_cast<std::size_t *>(malloc(std::max(
      sizeof(std::size_t) * num_items_per_thread, max_known_page_size)));
  // Assume first-touch NUMA, so actually do the initialization on the threads
  // before running the benchmark.
  std::thread pool[num_threads - 1uz];
  // reserve the 0th iteration for the main thread.
  std::size_t first_thread_items =
    num_reps_per_thread + (remainder ? 1uz : 0uz);
  for (std::size_t i = 0uz; i < num_threads - 1uz; i++) {
    pool[i] = std::thread(on_thread_baseline<init, update>,
                          map_ids[i + 1uz],
                          0uz,
                          ((i + 1uz) * num_items) / num_threads,
                          items[i],
                          num_items_per_thread,
                          (i + 1uz) < remainder ? num_reps_per_thread + 1uz
                                                : num_reps_per_thread);
  }
  for (std::size_t i = 0uz; i < num_items_per_thread; i++) items[0uz][i] = init;
  qtimer_t timer = qtimer_create();
  while (ready.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();
  qtimer_start(timer);
  ready.store(0uz, std::memory_order_relaxed);
  stress_non_atomics<update>(map_ids[0uz],
                             0uz,
                             0uz,
                             items[0uz],
                             num_items_per_thread,
                             first_thread_items);
  while (finished.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();
  finished.store(0uz, std::memory_order_relaxed);
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf("Baseline time for %zu non-atomic updates to %zu partitioned items "
         "with %zu threads: %f seconds\n",
         num_reps,
         num_items,
         num_threads,
         time);
  for (std::size_t i = 0uz; i < num_threads - 1uz; i++) pool[i].join();
  for (std::size_t i = 0uz; i < num_threads; i++) free(items[i]);
}

#endif
