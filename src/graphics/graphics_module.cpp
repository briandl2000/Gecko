#include "gecko/graphics/graphics_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"

namespace gecko::graphics {

namespace {

constexpr ::gecko::ServiceId RequiredServices[] = {
    ::gecko::ServiceIdOf<::gecko::ILogger>(),
    ::gecko::ServiceIdOf<::gecko::IProfiler>(),
    ::gecko::ServiceIdOf<::gecko::IJobSystem>(),
    ::gecko::ServiceIdOf<::gecko::IEventBus>(),
};

// IGpuSampler is *not* listed here -- it is published only when the
// device actually returns one (NullDevice does not, and other devices
// may legitimately decline). Advertising it unconditionally would make
// dependents assume it will exist after Startup and would block any
// other module from publishing IGpuSampler in those configs. Consumers
// that want a sampler look it up via the free accessor `GetGpuSampler()`
// or `IModuleRegistry::Service<IGpuSampler>()` and treat a null result
// as "no sampler available".
constexpr ::gecko::ServiceId PublishedServices[] = {
    ::gecko::ServiceIdOf<GraphicsDevice>(),
};

// File-scope pointers populated by Startup, cleared by Shutdown. Free
// accessors (GetGraphicsDevice / GetGpuSampler) read these.
GraphicsDevice* g_Device = nullptr;
IGpuSampler* g_Sampler = nullptr;

}  // namespace

GraphicsModule::GraphicsModule(const GraphicsConfig& config) noexcept : m_Config(config)
{}

GraphicsModule::GraphicsModule(const GraphicsConfig& config, Backends backends) noexcept
    : m_Config(config), m_Device(backends.Device)
{}

GraphicsModule::~GraphicsModule() noexcept = default;

::gecko::Span<const ::gecko::ServiceId> GraphicsModule::Requires() const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {RequiredServices};
}

::gecko::Span<const ::gecko::ServiceId> GraphicsModule::Publishes() const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {PublishedServices};
}

bool GraphicsModule::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_SCOPE(labels::Graphics);

  if (!m_Device)
  {
    GraphicsDeviceDesc desc {};
    desc.Backend = m_Config.Backend;
    desc.Debug = m_Config.Debug;
    desc.AppName = m_Config.AppName;
    m_OwnedDevice = CreateGraphicsDevice(desc);
    m_Device = m_OwnedDevice.get();
  }
  if (!m_Device)
  {
    GECKO_ERROR(labels::Graphics, "Failed to create GraphicsDevice");
    return false;
  }

  // Optional sampler -- NullDevice returns null and that is fine.
  m_OwnedSampler = m_Device->CreateGpuSampler(m_Config.Sampler);

  if (!modules.PublishService<GraphicsDevice>(m_Device))
  {
    GECKO_ERROR(labels::Graphics, "PublishService<GraphicsDevice> failed");
    return false;
  }
  if (m_OwnedSampler)
  {
    if (!modules.PublishService<IGpuSampler>(m_OwnedSampler.get()))
    {
      GECKO_ERROR(labels::Graphics, "PublishService<IGpuSampler> failed");
      (void)modules.UnpublishService<GraphicsDevice>();
      return false;
    }
  }

  g_Device = m_Device;
  g_Sampler = m_OwnedSampler.get();

  return true;
}

void GraphicsModule::Shutdown(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_SCOPE(labels::Graphics);

  g_Sampler = nullptr;
  g_Device = nullptr;

  if (m_OwnedSampler)
    (void)modules.UnpublishService<IGpuSampler>();
  (void)modules.UnpublishService<GraphicsDevice>();

  // Sampler must be destroyed before the device (it holds GPU
  // resources owned by the device).
  m_OwnedSampler.reset();
  m_Device = nullptr;
  m_OwnedDevice.reset();
}

// -- Free function accessors -----------------------------------------

GraphicsDevice* GetGraphicsDevice() noexcept
{
  return g_Device;
}

IGpuSampler* GetGpuSampler() noexcept
{
  return g_Sampler;
}

}  // namespace gecko::graphics
