#include "gecko/platform/threading.h"

#include "gecko/core/services.h"
#include "gecko/core/services/modules.h"

namespace gecko::platform {

namespace {

// Process-wide fallback used whenever the engine has not booted a
// PlatformModule. Stateless; safe to share by pointer across threads.
NullThreading s_NullThreading;

}  // namespace

IThreading* GetThreading() noexcept
{
  if (auto* modules = ::gecko::GetModules())
  {
    if (auto* impl = modules->Service<IThreading>())
      return impl;
  }
  return &s_NullThreading;
}

}  // namespace gecko::platform
