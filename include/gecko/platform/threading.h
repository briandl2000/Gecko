#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko::platform {

// Stable, opaque identifier for the calling OS thread. Comparable for
// equality only; the underlying value is the native OS thread id (pid_t
// on Linux, DWORD on Windows) widened to 64 bits.
using ThreadId = ::gecko::u64;

// Platform-provided threading service. Currently scoped to information
// queries and sleep/yield primitives; full Thread / Mutex types will be
// added in a follow-up ticket (see docs/tickets/PLATFORM_THREADING.md).
//
// Published by PlatformModule. Access via ::gecko::platform::GetThreading()
// which never returns null (falls back to NullThreading when the engine
// has not booted the platform module).
struct IThreading
{
  GECKO_API virtual ~IThreading() = default;

  // Hardware capability ----------------------------------------------------

  // Number of logical processors visible to the process. Always >= 1.
  // The value is cached at module startup; cheap to call repeatedly.
  [[nodiscard]] GECKO_API virtual ::gecko::u32 GetHardwareThreadCount()
      const noexcept = 0;

  // Per-thread queries -----------------------------------------------------

  // Native OS id of the calling thread. Stable for the lifetime of the
  // thread, unique within the process.
  [[nodiscard]] GECKO_API virtual ThreadId GetCurrentThreadId()
      const noexcept = 0;

  // Set a debugger-visible name on the calling thread. `name` is copied
  // immediately; safe to free after the call. No-op if the platform
  // does not support thread names. Names longer than the platform limit
  // (15 chars on Linux pthread) are truncated.
  GECKO_API virtual void SetCurrentThreadName(const char* name) noexcept = 0;

  // Sleep / yield ----------------------------------------------------------

  // Block the calling thread for at least `nanoseconds`. Resolution is
  // platform-dependent; on Windows, PlatformModule requests 1ms timer
  // resolution at startup so sub-millisecond sleeps are usable but not
  // exact. A zero value is a no-op.
  GECKO_API virtual void SleepNanoseconds(
      ::gecko::u64 nanoseconds) const noexcept = 0;

  // Hint to the scheduler that the calling thread is willing to release
  // the rest of its quantum.
  GECKO_API virtual void YieldThread() const noexcept = 0;
};

// Default fallback used when no PlatformModule is active. Reports a
// single logical processor and treats sleep/yield as no-ops; suitable
// for unit tests that never construct a PlatformModule.
struct NullThreading final : IThreading
{
  [[nodiscard]] GECKO_API virtual ::gecko::u32 GetHardwareThreadCount()
      const noexcept override
  {
    return 1;
  }

  [[nodiscard]] GECKO_API virtual ThreadId GetCurrentThreadId()
      const noexcept override
  {
    return 0;
  }

  GECKO_API virtual void SetCurrentThreadName(
      const char* /*name*/) noexcept override
  {}

  GECKO_API virtual void SleepNanoseconds(
      ::gecko::u64 /*nanoseconds*/) const noexcept override
  {}

  GECKO_API virtual void YieldThread() const noexcept override
  {}
};

// Returns the IThreading implementation published by the active module
// registry, or a NullThreading fallback if no PlatformModule has been
// started. Never returns null.
[[nodiscard]] GECKO_API IThreading* GetThreading() noexcept;

}  // namespace gecko::platform
