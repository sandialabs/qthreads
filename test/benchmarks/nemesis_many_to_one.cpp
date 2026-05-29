#include <cstdio>
#include <cstdlib>

#include <qthread/common.h>
#include <qthread/qtimer.h>

#include <qt_atomics.h>

#include "argparsing.h"

#include "nemesis_mini.hpp"

using queue_t = nemesis_queue<std::size_t>;
using item_t = queue_t::item_t;

void producer_thread(std::atomic<bool> *ready,
                     queue_t *queue,
                     std::size_t items_per_thread,
                     item_t *item_buffer) noexcept {
  while (!ready->load(std::memory_order_relaxed)) SPINLOCK_BODY();
  for (std::size_t i = 0ull; i < items_per_thread; i++) {
    item_t *item = new (&item_buffer[i]) item_t{i + 1ull};
    queue->enqueue_existing(item);
  }
}

std::size_t arithmetic_sum(std::size_t val) {
  if (val % 2ull) {
    return ((val - 1ull) / 2ull + 1ull) * val;
  } else {
    return (val / 2ull) * (val + 1ull);
  }
}

int main() {
  std::size_t num_items = 1000000ull;
  std::size_t num_threads = 4u;
  NUMARG(num_items, "NUM_ITEMS");
  NUMARG(num_threads, "NUM_THREADS");
  std::size_t num_items_per_thread = num_items / num_threads;
  std::size_t remainder = num_items % num_threads;
  std::atomic<bool> ready{false};
  std::thread pool[num_threads];
  // Defer proper initialization to inside each worker thread.
  item_t *items = (item_t *)malloc(sizeof(item_t) * num_items);
  queue_t queue;
  qtimer_t timer = qtimer_create();
  qtimer_start(timer);
  std::size_t current = 0ull;
  for (std::size_t i = 0ull; i < num_threads; i++) {
    std::size_t items_for_this_thread =
      num_items_per_thread + std::size_t(i < remainder);
    pool[i] = std::thread(
      producer_thread, &ready, &queue, items_for_this_thread, items + current);
    current += items_for_this_thread;
  }
  ready.store(true, std::memory_order_relaxed);
  qtimer_start(timer);
  std::size_t i = 0ull;
  std::size_t sum = 0ull;
  while (i < num_items) {
    std::size_t fail_count = 0ull;
    item_t *item = queue.dequeue_single();
    if (item) {
      sum += item->value;
      i++;
    } else {
      fail_count++;
      if (fail_count >= 10000) { printf("failed at index: %lu\n", i); }
    }
  }
  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  printf("Time for running %lu work items from %lu distinct threads through a "
         "nemesis queue to a single consumer is: %f seconds\n",
         num_items,
         num_threads,
         time);
  for (std::size_t i = 0ull; i < num_threads; i++) { pool[i].join(); }
  free(items);
  std::size_t expected = arithmetic_sum(num_items_per_thread) * (num_threads) +
                         remainder * (num_items_per_thread + 1uz);
  if (expected != sum) std::abort();
}

