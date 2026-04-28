#include "App.h"

#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>

namespace app::debug_renderer_example {

namespace labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.debug_renderer_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.debug_renderer_example.main");
}  // namespace labels

App::AllocatorInstaller::AllocatorInstaller(::gecko::IAllocator* a) noexcept
    : Ok(::gecko::SetAllocator(a))
{}

App::AllocatorInstaller::~AllocatorInstaller()
{
  ::gecko::ResetAllocator();
}

::gecko::Label App::AppModule::RootLabel() const noexcept
{
  return labels::App;
}

bool App::AppModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::AppModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App()
    : m_RuntimeModule(m_JobSystem, m_Profiler, m_Logger, m_EventBus)
{
  if (!m_AllocatorInstaller.Ok)
    return;

  m_JobSystem.SetWorkerThreadCount(4);

  m_Engine = ::gecko::Engine::Create({&m_RuntimeModule, &m_AppModule});
  if (!m_Engine)
    return;

  AttachSinks();
  GECKO_INFO(labels::Main, "Gecko %s", ::gecko::VersionFullString());
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
  logger->SetLevel(::gecko::LogLevel::Info);
  m_SinksAttached = true;
}

void App::DetachSinks()
{
  m_ConsoleSink.Unregister();
  m_SinksAttached = false;
}

int App::Run()
{
  if (!IsValid())
    return 1;

  GECKO_INFO(labels::Main, "Hello from debug_renderer_example");
  GECKO_SLEEP_MS(16);
  return 0;
}

}  // namespace app::debug_renderer_example
