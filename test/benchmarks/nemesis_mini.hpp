#ifndef QT_MINI_NEMESIS_H
#define QT_MINI_NEMESIS_H

// A miniaturized version of the nemesis threadqueue usable for benchmarking.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <thread>

#define CACHE_LINE_SIZE 128

// TODO: for platforms with 128b atomics and mixed-size coherency
// (most of them as of this writing), test whether some of these
// operations can/should be shited to the 128-bit instructions.
// It likely doesn't matter since the synchronization
// is probably the main overhead and it's probably done at a
// more coarse level than 8-byte blocks, but it's good to test.

template <typename value_t>
struct nemesis_queue {
  static_assert(std::is_same_v<value_t, std::decay_t<value_t>>);
  static_assert(std::is_trivially_destructible_v<value_t>);
  static_assert(std::is_trivially_destructible_v<std::atomic<value_t>>);

  struct item_t {
    std::atomic<item_t *> next;
    std::atomic<value_t> value;
    item_t(item_t const &) = delete;
    item_t(item_t &&) = delete;

    item_t(value_t val) noexcept: next{nullptr}, value{val} {
      // Slight redundancy here between init
      // and then atomic writing the same value.
      // Using the constructor is for
      // standard conformity WRT initialization.
      // The atomic write prevents any implied
      // race because the default init
      // doesn't guarantee atomicity.
      // Trust the optimizer to clean up the duplication.
      next.store(nullptr, std::memory_order_relaxed);
      value.store(val, std::memory_order_relaxed);
    }
  };

  std::atomic<item_t *> head;
#ifdef HEAD_TAIL_PADDING
  char scratch_head_to_tail[CACHE_LINE_SIZE - sizeof(head)];
#endif
  std::atomic<item_t *> tail;

  nemesis_queue() noexcept: head(nullptr), tail(nullptr) {
    head.store(nullptr, std::memory_order_relaxed);
    tail.store(nullptr, std::memory_order_relaxed);
  }

  void enqueue_existing(item_t *item) noexcept {
    assert(!item->next.load(memory_order_relaxed) &&
           "Encountered null next pointer");
    item_t *old_tail = tail.exchange(item, std::memory_order_relaxed);
    if (old_tail) {
      old_tail->next.store(item, std::memory_order_relaxed);
    } else {
      head.store(item, std::memory_order_relaxed);
    }
  }

  item_t *dequeue_single() noexcept {
    item_t *item = head.load(std::memory_order_relaxed);
    if (!item) return nullptr;
    item_t *next = item->next.load(std::memory_order_relaxed);
    if (next) {
      head.store(next, std::memory_order_relaxed);
    } else {
      head.store(nullptr, std::memory_order_relaxed);
      item_t *tail_local = item;
      if (!tail.compare_exchange_strong(tail_local,
                                        nullptr,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
        next = item->next.load(std::memory_order_relaxed);
        while (!next) next = item->next.load(std::memory_order_relaxed);
        head.store(next, std::memory_order_relaxed);
      }
    }
    return item;
  }
};

#endif // QT_MINI_NEMESIS_H
