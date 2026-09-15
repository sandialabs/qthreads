#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unordered_set>

#include <qthread/qtimer.h>

#include "qt_atomics.h"

#include "argparsing.h"

static constexpr std::size_t sentinel = SIZE_MAX;
static std::atomic<std::size_t> ready{0z};
static std::atomic<std::size_t> finished{0z};
static std::atomic<std::size_t> tail{sentinel};

// A simplification of the communcation pattern that shows up when
// a bunch of threads are pushing work items into a single
// nemesis queue.
void run_swaps(std::atomic<std::size_t> *tail,
               std::size_t *items,
               std::size_t start,
               std::size_t num_items) noexcept {
  for (std::size_t i = start; i < start + num_items; i++) {
    items[i] = tail->exchange(i, std::memory_order_acq_rel);
  }
}

void on_thread(std::atomic<std::size_t> *tail,
               std::size_t *items,
               std::size_t start,
               std::size_t num_items) noexcept {
  // Indicate that this thread is ready, then wait for the start signal
  ready.fetch_add(1uz, std::memory_order_relaxed);
  while (ready.load(std::memory_order_relaxed)) SPINLOCK_BODY();

  // Run the atomic swap loop that's being benchmarked.
  run_swaps(tail, items, start, num_items);

  // Indicate that this thread is finished.
  finished.fetch_add(1uz, memory_order_relaxed);
}

int main(int argc, char *argv[]) {
  std::size_t num_threads = 4uz, num_items = 1000000000uz;
  NUMARG(num_threads, "NUM_THREADS");
  NUMARG(num_items, "NUM_ITEMS");
  std::size_t num_items_per_thread = num_items / num_threads;
  std::size_t remainder = num_items % num_threads;
  std::size_t *items =
    reinterpret_cast<std::size_t *>(malloc(num_items * sizeof(size_t)));
  qtimer_t timer = qtimer_create();
  std::thread pool[num_threads - 1uz];
  // reserve the 0th iteration for the main thread.
  std::size_t first_thread_items =
    num_items_per_thread + (remainder ? 1uz : 0uz);
  std::size_t current_start = first_thread_items;
  for (std::size_t i = 0uz; i < num_threads - 1uz; i++) {
    std::size_t items_for_this_thread =
      num_items_per_thread + (i + 1uz < remainder ? 1uz : 0uz);
    pool[i] = std::thread(
      on_thread, &tail, items, current_start, items_for_this_thread);
    current_start += items_for_this_thread;
  }
  // Wait for all threads to be ready
  printf("waiting for threads\n");
  while (ready.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();
  printf("signaling threads\n");
  // Signal the start to all the threads.
  qtimer_start(timer);
  ready.store(0uz, std::memory_order_relaxed);

  printf("running main thread work\n");
  // have main thread do the thread 0 work
  run_swaps(&tail, items, 0uz, first_thread_items);

  // Wait for all threads to finish
  printf("waiting for workers to finish\n");
  while (finished.load(std::memory_order_relaxed) < num_threads - 1uz)
    SPINLOCK_BODY();

  qtimer_stop(timer);
  double time = qtimer_secs(timer);
  qtimer_destroy(timer);

  for (uint64_t i = 0uz; i < num_threads - 1uz; i++) pool[i].join();
  free(items);

  printf("Time for %lu contended acq_rel swaps by %lu threads: %f seconds.\n",
         num_items,
         num_threads,
         time);
}
