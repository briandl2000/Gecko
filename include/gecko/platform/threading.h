#pragma once

/// @file
/// OS-flavoured threading queries: hardware count, native thread id,
/// thread name, sleep, yield.
///
/// Stateless namespace functions; backed by pthread on Linux and
/// Win32 on Windows, selected at compile time. For pure-std generic
/// helpers (`std::thread::hardware_concurrency` wrappers, hashed
/// `std::thread::id`, etc.) see `gecko/core/utility/thread.h`.

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko::platform {

/// Stable, opaque identifier for the calling OS thread. The underlying
/// value is the native OS thread id (`gettid()` on Linux,
/// `GetCurrentThreadId` on Windows) widened to 64 bits.
using ThreadId = ::gecko::u64;

/// Number of logical processors visible to the process. Always `>= 1`.
[[nodiscard]] GECKO_API ::gecko::u32 HardwareThreadCount() noexcept;

/// Native OS id of the calling thread. Stable for the lifetime of the
/// thread, unique within the process.
[[nodiscard]] GECKO_API ThreadId CurrentThreadId() noexcept;

/// Set a debugger-visible name on the calling thread. Names longer
/// than the platform limit (15 chars on Linux pthread) are truncated;
/// `nullptr` is a no-op.
/// @param name  C-string thread name, or `nullptr`.
GECKO_API void SetCurrentThreadName(const char* name) noexcept;

/// Block the calling thread for at least `nanoseconds`. Resolution is
/// platform-dependent; on Windows, `PlatformModule` requests 1 ms timer
/// resolution at startup. A zero value is a no-op.
GECKO_API void SleepNanoseconds(::gecko::u64 nanoseconds) noexcept;

/// Hint to the scheduler that the calling thread is willing to release
/// the rest of its quantum.
GECKO_API void YieldThread() noexcept;

}  // namespace gecko::platform
