#include "App.h"

#include "Demos.h"
#include "Labels.h"

#include <cstdio>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>

namespace gecko::examples::core_example {

::gecko::Label App::ExampleModule::RootLabel() const noexcept
{
  return labels::App;
}

bool App::ExampleModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::ExampleModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App()
{
  if (!m_AllocScope)
    return;

  m_Engine = ::gecko::Engine::Create({&m_RuntimeModule, &m_AppModule});
  if (!m_Engine)
    return;

  m_LogSinks.emplace();

  // Force Detailed profiler events so users can inspect every emitted
  // event in Perfetto / chrome://tracing when running this example.
  if (auto* profiler = ::gecko::GetProfiler())
    profiler->SetMinLevel(::gecko::ProfLevel::Detailed);
  m_TraceSink.SetMinLevel(::gecko::ProfLevel::Detailed);

  if (m_TraceSink.IsOpen())
  {
    if (auto* profiler = ::gecko::GetProfiler())
    {
      m_TraceSink.RegisterWith(profiler);
      profiler->SetTraceEnabled(true);
    }
  }
  else
  {
    GECKO_WARN(labels::Main, "Failed to open trace profiler sink");
  }

  ::gecko::SetThreadProfilerName("main");
  GECKO_INFO(labels::Main, ::gecko::VersionFullString());
}

int App::Run()
{
  if (!IsValid())
    return 1;

  GECKO_SCOPE(labels::Main);
  GECKO_INFO(labels::Main, "Starting Core feature tour");

  // Re-enable Trace level so demo output is visible to the reader.
  if (auto* logger = ::gecko::GetLogger())
    logger->SetLevel(::gecko::LogLevel::Trace);

  GECKO_INFO(labels::Main, "=== 1. Memory Management ===");
  demos::RunMemory(m_Allocator);

  GECKO_INFO(labels::Main, "=== 2. Event System ===");
  demos::RunEvents();

  GECKO_INFO(labels::Main, "=== 3. Threading Utilities ===");
  demos::RunThreading();

  GECKO_INFO(labels::Main, "=== 4. Job System ===");
  demos::RunJobs(m_Allocator);

  GECKO_INFO(labels::Main, "=== 5. Log Levels ===");
  demos::RunLogging();

  GECKO_INFO(labels::Main, "=== 6. Profiler Diagnostics ===");
  demos::RunProfilerDiagnostics();

  GECKO_FRAME(labels::Main, "EndOfDemo");
  return 0;
}

}  // namespace gecko::examples::core_example
