#pragma once

#if defined(GECKO_PLATFORM_WINDOWS)

#include "gecko/platform/path_view.h"

#include <cstddef>
#include <string>

namespace gecko::platform::win32_io {

// Convert a forward-slash UTF-8 PathView to the native UTF-16 form
// with backslashes. Returns empty on conversion failure.
[[nodiscard]] ::std::wstring ToWide(PathView path) noexcept;

// Convert a UTF-16 path back to UTF-8 with forward slashes.
[[nodiscard]] ::std::string FromWide(const wchar_t* wide,
                                     ::std::size_t wLen) noexcept;

}  // namespace gecko::platform::win32_io

#endif  // GECKO_PLATFORM_WINDOWS
