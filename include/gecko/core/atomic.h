#pragma once

#include "gecko/core/types.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gecko {

namespace detail {

#if defined(_MSC_VER)

inline u32 AtomicLoad(const u32* address) noexcept
{
  return static_cast<u32>(
      _InterlockedCompareExchange(reinterpret_cast<volatile long*>(const_cast<u32*>(address)), 0, 0));
}
inline void AtomicStore(u32* address, u32 value) noexcept
{
  (void)_InterlockedExchange(reinterpret_cast<volatile long*>(address), static_cast<long>(value));
}
inline u32 AtomicExchange(u32* address, u32 value) noexcept
{
  return static_cast<u32>(_InterlockedExchange(reinterpret_cast<volatile long*>(address), static_cast<long>(value)));
}
inline u32 AtomicIncrement(u32* address) noexcept
{
  return static_cast<u32>(_InterlockedIncrement(reinterpret_cast<volatile long*>(address)));
}
inline u32 AtomicDecrement(u32* address) noexcept
{
  return static_cast<u32>(_InterlockedDecrement(reinterpret_cast<volatile long*>(address)));
}
inline void AtomicCpuRelax() noexcept
{
#if defined(_M_X64) || defined(_M_IX86)
  _mm_pause();
#endif
}

#else

inline u32 AtomicLoad(const u32* address) noexcept
{
  return __atomic_load_n(address, __ATOMIC_ACQUIRE);
}
inline void AtomicStore(u32* address, u32 value) noexcept
{
  __atomic_store_n(address, value, __ATOMIC_RELEASE);
}
inline u32 AtomicExchange(u32* address, u32 value) noexcept
{
  return __atomic_exchange_n(address, value, __ATOMIC_ACQ_REL);
}
inline u32 AtomicIncrement(u32* address) noexcept
{
  return __atomic_add_fetch(address, 1U, __ATOMIC_RELAXED);
}
inline u32 AtomicDecrement(u32* address) noexcept
{
  return __atomic_sub_fetch(address, 1U, __ATOMIC_ACQ_REL);
}
inline void AtomicCpuRelax() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#endif
}

#endif

}  // namespace detail

/// Small compiler-atomic primitive used to build Gecko's synchronization
/// types without exposing compiler intrinsics throughout the engine.
class AtomicU32
{
public:
  constexpr AtomicU32() noexcept = default;
  constexpr explicit AtomicU32(u32 value) noexcept : m_Value(value)
  {}

  AtomicU32(const AtomicU32&) = delete;
  AtomicU32& operator=(const AtomicU32&) = delete;

  [[nodiscard]] u32 Load() const noexcept
  {
    return detail::AtomicLoad(&m_Value);
  }
  void Store(u32 value) noexcept
  {
    detail::AtomicStore(&m_Value, value);
  }
  [[nodiscard]] u32 Exchange(u32 value) noexcept
  {
    return detail::AtomicExchange(&m_Value, value);
  }
  [[nodiscard]] u32 Increment() noexcept
  {
    return detail::AtomicIncrement(&m_Value);
  }
  [[nodiscard]] u32 Decrement() noexcept
  {
    return detail::AtomicDecrement(&m_Value);
  }

private:
  alignas(sizeof(u32)) mutable u32 m_Value {0};
};

inline void CpuRelax() noexcept
{
  detail::AtomicCpuRelax();
}

}  // namespace gecko
