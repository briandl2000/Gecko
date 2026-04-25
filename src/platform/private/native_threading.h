#pragma once

#include "gecko/core/ptr.h"
#include "gecko/platform/threading.h"

namespace gecko::platform {

// Creates the IThreading implementation appropriate for the current
// build target. Resolves at link time:
//   - Linux build: pthread / clock_nanosleep backend
//   - Windows build: Win32 / Sleep backend
//   - Otherwise: returns NullThreading
//
// Always returns a non-null pointer. Caller (PlatformModule) owns it.
[[nodiscard]] ::gecko::Unique<IThreading> CreateNativeThreading() noexcept;

}  // namespace gecko::platform
