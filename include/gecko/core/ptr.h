#pragma once

/// @file
/// Smart-pointer aliases and convenience factories.
///
/// Gecko APIs use `Unique<T>`, `Shared<T>`, and `Weak<T>` (thin
/// aliases for the standard library equivalents) so call sites read
/// without `::std::` noise and so the project can swap the underlying
/// implementation later if needed.

#include <memory>

namespace gecko {

// ------------------------------------------------------------
// Smart pointer aliases
// ------------------------------------------------------------

/// Unique-ownership smart pointer (`std::unique_ptr`).
template <typename T>
using Unique = ::std::unique_ptr<T>;

/// Shared-ownership smart pointer (`std::shared_ptr`).
template <typename T>
using Shared = ::std::shared_ptr<T>;

/// Non-owning observer of a `Shared<T>` (`std::weak_ptr`).
template <typename T>
using Weak = ::std::weak_ptr<T>;

// ------------------------------------------------------------
// Smart pointer creation
// ------------------------------------------------------------

/// Construct a `Unique<T>` in-place (forwards to `std::make_unique`).
template <typename T, typename... Args>
[[nodiscard]]
constexpr Unique<T> CreateUnique(Args&&... args)
{
  return ::std::make_unique<T>(::std::forward<Args>(args)...);
}

/// Construct a `Shared<T>` in-place (forwards to `std::make_shared`).
template <typename T, typename... Args>
[[nodiscard]]
constexpr Shared<T> CreateShared(Args&&... args)
{
  return ::std::make_shared<T>(::std::forward<Args>(args)...);
}

/// Wrap an already-allocated raw pointer in a `Unique<T>`. Ownership
/// transfers to the returned smart pointer.
template <typename T>
[[nodiscard]]
constexpr Unique<T> CreateUniqueFromRaw(T* t)
{
  return ::std::unique_ptr<T>(t);
}

/// Wrap an already-allocated raw pointer in a `Shared<T>`. Ownership
/// transfers to the returned smart pointer.
template <typename T>
[[nodiscard]]
constexpr Shared<T> CreateSharedFromRaw(T* t)
{
  return ::std::shared_ptr<T>(t);
}

/// Make a non-owning `Weak<T>` from an existing `Shared<T>`.
template <typename T>
[[nodiscard]]
constexpr Weak<T> CreateWeakFromShared(Shared<T> shared)
{
  return ::std::weak_ptr<T>(shared);
}
}  // namespace gecko
