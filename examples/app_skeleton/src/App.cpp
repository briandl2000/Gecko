#include "App.h"

#include <cstdio>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/platform/platform_events.h>
#include <gecko/platform/windows_interface.h>

namespace gecko::examples::app_skeleton {

namespace labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.app_skeleton");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.app_skeleton.main");
}  // namespace labels

App::AllocatorInstaller::AllocatorInstaller(::gecko::IAllocator* a) noexcept
    : Ok(::gecko::SetAllocator(a))
{}

App::AllocatorInstaller::~AllocatorInstaller()
{
  ::gecko::ResetAllocator();
}

::gecko::Label App::SkeletonModule::RootLabel() const noexcept
{
  return labels::App;
}

bool App::SkeletonModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::SkeletonModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App(const AppConfig& cfg)
    : m_Config(cfg), m_JobSystem(),
      m_RuntimeModule(m_JobSystem, m_Profiler, m_Logger, m_EventBus),
      m_PlatformModule(::gecko::platform::PlatformConfig {
          .Backend = cfg.backend, .Window = {}, .Monitor = {}})
{
  if (!m_AllocatorInstaller.Ok)
    return;

  m_JobSystem.SetWorkerThreadCount(4);

  m_Engine = ::gecko::Engine::Create(
      {&m_RuntimeModule, &m_PlatformModule, &m_AppModule});
  if (!m_Engine)
    return;

  AttachSinks();
  GECKO_INFO(labels::Main, ::gecko::VersionFullString());
}

App::~App()
{
  if (m_SinksAttached)
    DetachSinks();
  m_Engine.reset();
}

void App::AttachSinks()
{
  auto* logger = ::gecko::GetLogger();
  if (!logger)
    return;
  m_ConsoleSink.RegisterWith(logger);
  m_FileSink.RegisterWith(logger);
  logger->SetLevel(::gecko::LogLevel::Info);
  m_SinksAttached = true;
}

void App::DetachSinks()
{
  m_ConsoleSink.Unregister();
  m_FileSink.Unregister();
  m_SinksAttached = false;
}

int App::Run()
{
  if (!IsValid())
    return 1;

  if (m_Config.windowed)
    RunWindowed();
  else
    RunHeadless();
  return 0;
}

void App::RunHeadless()
{
  GECKO_INFO(labels::Main, "Running headless");
  // Replace this with your own headless work.
  GECKO_SLEEP_MS(10);
}

void App::RunWindowed()
{
  using namespace ::gecko::platform;

  WindowDesc desc {};
  desc.Title = m_Config.title;
  desc.Size = {1280, 720};
  desc.Visible = true;
  desc.Resizable = true;

  WindowHandle window = GetWindows()->CreateWindow(desc);
  if (!window.IsValid())
  {
    GECKO_ERROR(labels::Main, "Failed to create window");
    return;
  }

  bool running = true;
  ::gecko::u32 frames = 0;

  auto closeSub = ::gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView) {
        *static_cast<bool*>(user) = false;
      },
      &running);

  while (running && GetWindows()->IsWindowAlive(window))
  {
    GECKO_SCOPE_NAMED(labels::Main, "Frame");

    PumpEvents();
    (void)::gecko::DispatchEvents();

    // Replace this with your own update/render work.
    GECKO_SLEEP_MS(16);

    ++frames;
    if (m_Config.maxFrames != 0 && frames >= m_Config.maxFrames)
      running = false;
  }

  GetWindows()->DestroyWindow(window);
}

}  // namespace gecko::examples::app_skeleton
