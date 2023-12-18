#pragma once

#include <atomic>
#include <cstddef>

namespace zeus {
#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
static constexpr size_t hw_inf_size =
    std::hardware_destructive_interference_size;
#else
static constexpr size_t hw_inf_size = 64;
#endif

template <typename T, typename... Args>
concept arg_regular_type = std::is_nothrow_constructible<T, Args...>::value &&
                           std::is_nothrow_destructible<T>::value;

/// Represents a slot in zeus::queue message queue, managing the lifecycle of
/// contained objects.
template <arg_regular_type T>
class slot {
 public:
  /// Destructor ensuring proper destruction of the contained object if
  /// necessary.
  ~slot() noexcept {
    if (turn % 2 != 0) {
      destroy();
    }
  }

  /// Constructs an object of type T in-place with the given arguments, ensuring
  /// no-throw construction.
  template <typename... A>
  void construct(A&&... arguments) noexcept {
    new (&storage) T(std::forward<A>(arguments)...);
  }

  /// Destroys the contained object of type T, ensuring it's no-throw
  /// destructible.
  void destroy() noexcept { reinterpret_cast<T*>(&storage)->~T(); }

  /// Moves the contained object out of the slot, returning a rvalue reference
  /// to it.
  T&& move() noexcept { return reinterpret_cast<T&&>(storage); }

 public:
  alignas(hw_inf_size) std::atomic_size_t turn = 0;
  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
};
}  // namespace zeus
