#if defined(GECKO_PLATFORM_WINDOWS)

#include "gecko/platform/shared_library.h"
#include "gecko/core/format.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace gecko::platform {

namespace {
thread_local char g_Error[64] = "no shared-library error";
}

SharedLibrary LoadSharedLibrary(const char* path) noexcept
{
  return SharedLibrary {.Handle = reinterpret_cast<void*>(::LoadLibraryA(path))};
}

void UnloadSharedLibrary(SharedLibrary library) noexcept
{
  if (library.IsValid())
    (void)::FreeLibrary(reinterpret_cast<HMODULE>(library.Handle));
}

void* FindSharedLibrarySymbol(SharedLibrary library, const char* symbol) noexcept
{
  if (!library.IsValid() || symbol == nullptr)
    return nullptr;
  return reinterpret_cast<void*>(::GetProcAddress(reinterpret_cast<HMODULE>(library.Handle), symbol));
}

const char* SharedLibraryError() noexcept
{
  const DWORD error = ::GetLastError();
  if (error == 0)
    return "no shared-library error";
  FormatBuffer buffer {.Data = g_Error, .Capacity = sizeof(g_Error)};
  FormatTo(buffer, "Win32 error {}", error);
  return g_Error;
}

}  // namespace gecko::platform

#endif
