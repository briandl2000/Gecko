#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

class PlatformContext
{
public:
  PlatformContext(const PlatformConfig& cfg);

  GECKO_API ~PlatformContext() = default;

  GECKO_API virtual IWindowsBackend& Windows()
  {
    return *m_Windows;
  };

  GECKO_API virtual void PumpEvents() noexcept {};

private:
  Unique<IWindowsBackend> m_Windows;
};

}  // namespace gecko::platform
