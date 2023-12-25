#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "zeus/slot.hpp"

namespace zeus {
template <typename T>
concept regular_type = (std::is_nothrow_copy_assignable<T>::value ||
                        std::is_nothrow_move_assignable<T>::value) &&
                       std::is_nothrow_destructible<T>::value;

/// Thread-safe, lock-free queue supporting multiple producers and consumers.
template <regular_type T>
class queue {
 public:
  /// Constructs a new queue holding items of type T with the provided capacity
  explicit queue(const std::size_t capacity) : max_capacity(capacity) {
    // Add an extra slot to avoid incorrect data overlap in the final slot
    slots = std::make_unique<zeus::slot<T>[]>(max_capacity + 1);

    // Ensure that alignment is honored for over-aligned types
    if (reinterpret_cast<std::size_t>(slots.get()) % alignof(zeus::slot<T>) !=
        0) {
      throw std::bad_alloc();
    }

    for (std::size_t i = 0; i < max_capacity; ++i) {
      new (&slots[i]) zeus::slot<T>();
    }
  }

  /// Destroys the queue, releasing all resources.
  ~queue() noexcept {
    for (std::size_t i = 0; i < max_capacity; ++i) {
      slots[i].~slot();
    }
  }

  queue(const queue&) = delete;
  queue& operator=(const queue&) = delete;

  /// Enqueues an item by in-place construction, blocking if the queue is full.
  template <typename... A>
    requires std::is_nothrow_constructible<T, A&&...>::value
  void emplace(A&&... arguments) noexcept {
    const auto current_head = head.fetch_add(1);
    zeus::slot<T>& slot = slots[get_idx(current_head)];

    // Block while waiting for an open turn
    do {
    } while (get_current_turn(current_head) * 2 !=
             slot.turn.load(std::memory_order_acquire));

    slot.construct(std::forward<A>(arguments)...);
    slot.turn.store(get_current_turn(current_head) * 2 + 1,
                    std::memory_order_release);
  }

  /// Attempts to enqueue an item by in-place construction. Returns true on
  /// success, false if the queue is full.
  template <typename... A>
    requires std::is_nothrow_constructible<T, A&&...>::value
  bool try_emplace(A&&... arguments) noexcept {
    auto current_head = head.load(std::memory_order_acquire);

    while (true) {
      zeus::slot<T>& slot = slots[get_idx(current_head)];
      auto loaded_turn = slot.turn.load(std::memory_order_acquire);
      auto current_turn = get_current_turn(current_head) * 2;

      if (current_turn == loaded_turn) {
        // If the slot is at the correct turn, try to claim it
        if (head.compare_exchange_strong(current_head, current_head + 1)) {
          // Construct the item in place and update the turn
          slot.construct(std::forward<A>(arguments)...);
          slot.turn.store(current_turn + 1, std::memory_order_release);
          return true;
        }
      } else {
        // If the slot turn does not match, the queue might be full
        auto prev_head = current_head;
        current_head = head.load(std::memory_order_acquire);

        // If head hasn't changed, queue is full
        if (current_head == prev_head) {
          return false;
        }
      }
    }
  }

  /// Enqueues an item using move construction, blocking if the queue is full.
  template <typename P>
    requires std::is_nothrow_constructible<T, P&&>::value
  void push(P&& value) noexcept {
    emplace(std::forward<P>(value));
  }

  /// Enqueues an item using copy construction, blocking if the queue is full.
  void push(const T& value) noexcept
    requires std::is_nothrow_copy_constructible<T>::value
  {
    // Use the copy constructor to construct in-place
    emplace(value);
  }

  /// Tries to enqueue an item using copy construction. Returns true on success,
  /// false if the queue is full.
  bool try_push(const T& value) noexcept
    requires std::is_nothrow_copy_constructible<T>::value
  {
    return try_emplace(value);
  }

  /// Removes and returns the front item from the queue, blocking if the queue
  /// is full.
  T pop() noexcept
    requires std::is_nothrow_copy_constructible<T>::value
  {
    const auto current_tail = tail.fetch_add(1);
    zeus::slot<T>& slot = slots[get_idx(current_tail)];

    // Block while waiting for an open turn
    do {
    } while (get_current_turn(current_tail) * 2 + 1 !=
             slot.turn.load(std::memory_order_acquire));

    T rv(slot.move());
    slot.destroy();
    slot.turn.store(get_current_turn(current_tail) * 2 + 2,
                    std::memory_order_release);

    return rv;
  }

  /// Attempts to remove and return the front item from the queue. Returns the
  /// item if successful, or an empty optional if the queue is empty.
  std::optional<T> try_pop() noexcept {
    auto current_tail = tail.load(std::memory_order_acquire);

    while (true) {
      zeus::slot<T>& slot = slots[get_idx(current_tail)];
      auto loaded_turn = slot.turn.load(std::memory_order_acquire);
      auto expected_turn = get_current_turn(current_tail) * 2 + 1;

      if (expected_turn == loaded_turn) {
        // If the slot is at the expected turn, try to claim it
        if (tail.compare_exchange_strong(current_tail, current_tail + 1)) {
          T rv = slot.move();
          slot.destroy();
          slot.turn.store(get_current_turn(current_tail) * 2 + 2,
                          std::memory_order_relaxed);
          return rv;
        }
        // Loop continues with updated 't' if compare_exchange_strong fails
      } else {
        // If the slot turn does not match, the queue might be full
        auto prev_tail = current_tail;
        current_tail = tail.load(std::memory_order_acquire);

        // If tail hasn't changed, queue is full
        if (current_tail == prev_tail) {
          return std::nullopt;
        }
      }
    }
  }

  /// Returns the number of elements currently in the queue. The size may be
  /// negative when there is, at least, one reader waiting. The size is not
  /// guaranteed to be accurate.
  std::ptrdiff_t size() const noexcept {
    auto difference = head.load(std::memory_order_relaxed) -
                      tail.load(std::memory_order_relaxed);
    return static_cast<std::ptrdiff_t>(difference);
  }

  /// Returns true if the queue is empty, otherwise false.
  bool empty() const noexcept { return size() <= 0; }

 private:
  /// Returns the index in the queue corresponding to the global index i.
  constexpr std::size_t get_idx(std::size_t i) const noexcept {
    return i % max_capacity;
  }

  /// Returns the current turn of the queue for the global index i.
  constexpr std::size_t get_current_turn(std::size_t i) const noexcept {
    return i / max_capacity;
  }

  const std::size_t max_capacity;
  std::unique_ptr<zeus::slot<T>[]> slots;

  alignas(hw_inf_size) std::atomic_size_t head;
  alignas(hw_inf_size) std::atomic_size_t tail;
};
}  // namespace zeus
