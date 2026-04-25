#pragma once

#include "gecko/core/ptr.h"
#include "gecko/platform/platform_io.h"

namespace gecko::platform {

// Creates the IPlatformIO implementation appropriate for the current
// build target. Resolves at link time to LinuxPlatformIO or
// Win32PlatformIO. Always returns non-null. Caller (PlatformModule)
// owns the returned object.
[[nodiscard]] ::gecko::Unique<IPlatformIO> CreateNativePlatformIO() noexcept;

}  // namespace gecko::platform
