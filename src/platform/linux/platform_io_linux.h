#pragma once

#if defined(GECKO_PLATFORM_LINUX)

#include "gecko/platform/path_view.h"

#include <string>

namespace gecko::platform::linux_io {

// Convert a forward-slash PathView to a NUL-terminated std::string
// suitable for passing to libc/syscalls. Linux uses '/' natively so
// no separator translation is needed.
[[nodiscard]] ::std::string ToCString(PathView path) noexcept;

}  // namespace gecko::platform::linux_io

#endif  // GECKO_PLATFORM_LINUX
