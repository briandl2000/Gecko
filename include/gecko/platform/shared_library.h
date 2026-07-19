#pragma once

#include "gecko/api.h"

namespace gecko::platform {

struct SharedLibrary
{
  void* Handle {nullptr};

  [[nodiscard]] constexpr bool IsValid() const noexcept
  {
    return Handle != nullptr;
  }
};

[[nodiscard]] GECKO_API SharedLibrary LoadSharedLibrary(const char* path) noexcept;
GECKO_API void UnloadSharedLibrary(SharedLibrary library) noexcept;
[[nodiscard]] GECKO_API void* FindSharedLibrarySymbol(SharedLibrary library, const char* symbol) noexcept;
[[nodiscard]] GECKO_API const char* SharedLibraryError() noexcept;

template <typename Function>
[[nodiscard]] Function FindSharedLibraryFunction(SharedLibrary library, const char* symbol) noexcept
{
  return reinterpret_cast<Function>(FindSharedLibrarySymbol(library, symbol));
}

}  // namespace gecko::platform
