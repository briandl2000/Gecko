#include "App.h"

#include "Demos.h"
#include "Labels.h"

namespace gecko::examples::core_example {

App::App() noexcept
{
  GeckoConfig config {};
  config.AppName = "Gecko Core Example";
  config.Platform.Backend = platform::DisplayBackendKind::Null;
  config.EnableGraphics = false;
  config.MinimumLogLevel = LogLevel::Trace;
  config.ProfilerLevel = ProfLevel::Detailed;
  m_Initialized = Initialize(config) == InitializeResult::Success;
  if (m_Initialized)
  {
    SetThreadProfilerName("main");
    GECKO_INFO(labels::Main, "Gecko {}", VersionFullString());
  }
}

App::~App() noexcept
{
  if (m_Initialized)
    Shutdown();
}

int App::Run() noexcept
{
  if (!IsValid())
    return 1;

  GECKO_PROFILE(labels::Main);
  GECKO_INFO(labels::Main, "starting Core feature tour");

  GECKO_INFO(labels::Main, "=== 1. Memory Management ===");
  demos::RunMemory();
  GECKO_INFO(labels::Main, "=== 2. Event System ===");
  demos::RunEvents();
  GECKO_INFO(labels::Main, "=== 3. Threading Utilities ===");
  demos::RunThreading();
  GECKO_INFO(labels::Main, "=== 4. Job System ===");
  demos::RunJobs();
  GECKO_INFO(labels::Main, "=== 5. Log Levels ===");
  demos::RunLogging();
  GECKO_INFO(labels::Main, "=== 6. Profiler Diagnostics ===");
  demos::RunProfilerDiagnostics();

  GECKO_FRAME(labels::Main, "EndOfDemo");
  DumpProfilerStats(labels::Main);
  return 0;
}

}  // namespace gecko::examples::core_example
