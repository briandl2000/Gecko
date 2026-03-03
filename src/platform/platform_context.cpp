#include "gecko/platform/platform_context.h"

namespace gecko::platform {

PlatformContext::PlatformContext(const PlatformConfig& cfg)
{
  m_Windows = IWindowsBackend::Create(cfg);
}

}  // namespace gecko::platform
