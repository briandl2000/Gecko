#include "gecko/platform/platform_context.h"

#include "gecko/platform/platform_module.h"

namespace gecko::platform {

PlatformContext::PlatformContext(const PlatformConfig& cfg)
{
  m_Config = Resolve(cfg);
  m_Emitter = gecko::CreateEmitterForModule(labels::Platform);
  m_Windows = IWindowsBackend::Create(m_Config);
  m_Monitors = IMonitorsBackend::Create(m_Config);
  m_Monitors->EnumerateMonitors();
}

}  // namespace gecko::platform
