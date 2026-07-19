#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"

#if defined(_MSC_VER)

void operator delete(void* memory) noexcept
{
  gecko::DeallocBytes(memory);
}

void operator delete(void* memory, gecko::usize) noexcept
{
  gecko::DeallocBytes(memory);
}

#else

extern "C" __attribute__((visibility("hidden"))) void GeckoDelete(void* memory) noexcept asm("_ZdlPv");
extern "C" __attribute__((visibility("hidden"))) void GeckoDeleteSized(void* memory, gecko::usize) noexcept
    asm("_ZdlPvm");

extern "C" void GeckoDelete(void* memory) noexcept
{
  gecko::DeallocBytes(memory);
}

extern "C" void GeckoDeleteSized(void* memory, gecko::usize) noexcept
{
  gecko::DeallocBytes(memory);
}

#endif

#if !defined(_MSC_VER)

extern "C" int __cxa_guard_acquire(gecko::u64* guard) noexcept
{
  constexpr gecko::u64 Initializing = 1ULL << 8U;
  for (;;)
  {
    const gecko::u64 state = __atomic_load_n(guard, __ATOMIC_ACQUIRE);
    if ((state & 0xFFU) != 0)
      return 0;
    if (state == 0)
    {
      gecko::u64 expected = 0;
      if (__atomic_compare_exchange_n(guard, &expected, Initializing, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return 1;
    }
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#endif

  }
}

extern "C" void __cxa_guard_release(gecko::u64* guard) noexcept
{
  __atomic_store_n(guard, 1ULL, __ATOMIC_RELEASE);
}

extern "C" void __cxa_guard_abort(gecko::u64* guard) noexcept
{
  __atomic_store_n(guard, 0ULL, __ATOMIC_RELEASE);
}

extern "C" [[noreturn]] void __cxa_pure_virtual() noexcept
{
  gecko::AssertFailure(gecko::AssertInfo {
      .Expression = "pure virtual function has an implementation",
      .Message = "Pure virtual function call",
      .File = __FILE__,
      .Function = __func__,
      .Line = __LINE__,
  });
}

#else

extern "C" [[noreturn]] int __cdecl _purecall()
{
  gecko::AssertFailure(gecko::AssertInfo {
      .Expression = "pure virtual function has an implementation",
      .Message = "Pure virtual function call",
      .File = __FILE__,
      .Function = __func__,
      .Line = __LINE__,
  });
}

#endif
