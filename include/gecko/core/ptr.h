#pragma once

#include <memory>

namespace gecko {
template <typename T> using Unique = std::unique_ptr<T>;

template <typename T> using Shared = std::shared_ptr<T>;

template <typename T> using Weak = std::weak_ptr<T>;

template <typename T, typename... Args>
[[nodiscard]]
constexpr Unique<T> CreateUnique(Args &&...args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
[[nodiscard]]
constexpr Shared<T> CreateShared(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
[[nodiscard]]
constexpr Unique<T> CreateUniqueFromRaw(T *t) {
  return std::unique_ptr<T>(t);
}

template <typename T>
[[nodiscard]]
constexpr Shared<T> CreateSharedFromRaw(T *t) {
  return std::shared_ptr<T>(t);
}

template <typename T>
[[nodiscard]]
constexpr Weak<T> CreateWeakFromShared(Shared<T> shared) {
  return std::weak_ptr<T>(shared);
}
} // namespace gecko
