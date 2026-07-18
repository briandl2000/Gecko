#if defined(GECKO_PLATFORM_LINUX)

#include "gecko/platform/shared_library.h"

#include <dlfcn.h>

namespace gecko::platform {

SharedLibrary LoadSharedLibrary(const char* path) noexcept
{
  return SharedLibrary {.Handle = ::dlopen(path, RTLD_NOW | RTLD_LOCAL)};
}

void UnloadSharedLibrary(SharedLibrary library) noexcept
{
  if (library.IsValid())
    (void)::dlclose(library.Handle);
}

void* FindSharedLibrarySymbol(SharedLibrary library, const char* symbol) noexcept
{
  if (!library.IsValid() || symbol == nullptr)
    return nullptr;
  return ::dlsym(library.Handle, symbol);
}

const char* SharedLibraryError() noexcept
{
  const char* error = ::dlerror();
  return error != nullptr ? error : "no shared-library error";
}

}  // namespace gecko::platform

#endif
