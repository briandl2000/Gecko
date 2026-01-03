#include <gecko/core/boot.h>
#include <gecko/core/category.h>
#include <gecko/core/log.h>
#include <gecko/core/profiler.h>
#include <gecko/core/ptr.h>
#include <gecko/core/services.h>
#include <gecko/core/thread.h>
#include <gecko/core/version.h>
#include <gecko/platform/platform_context.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/trace_file_sink.h>
#include <gecko/runtime/tracking_allocator.h>

using namespace gecko;
using namespace gecko::platform;

const auto MAIN_CAT = MakeCategory("Main");

int main() {
  SystemAllocator systemAlloc;
  runtime::TrackingAllocator trackingAlloc(&systemAlloc);
  runtime::RingProfiler ringProfiler(1 << 16); // 64K events
  runtime::RingLogger ringLogger(1024); // 1024 log entries in ring buffer

  // Create job system with 4 worker threads
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  // Use GECKO_BOOT system for proper service installation and validation
  // Services are in dependency order: Allocator -> JobSystem -> Profiler ->
  // Logger
  GECKO_BOOT((Services{.Allocator = &trackingAlloc,
                       .JobSystem = &jobSystem,
                       .Profiler = &ringProfiler,
                       .Logger = &ringLogger}));

  // Now configure logging sinks after services are installed
  runtime::ConsoleLogSink consoleSink;
  runtime::FileLogSink fileSink("log.txt");

  if (auto *logger = GetLogger()) {
    logger->AddSink(&fileSink);
    logger->AddSink(&consoleSink);
    logger->SetLevel(
        LogLevel::Info); // Filter out Trace and Debug messages initially
  }

  gecko::LogVersion(MAIN_CAT);

  // Set up trace file sink for profiling data after services are available
  runtime::TraceFileSink traceSink("gecko_trace.json");
  if (!traceSink.IsOpen()) {
    GECKO_WARN(MAIN_CAT, "Failed to open trace profiler sink\n");
  } else {
    // We know the profiler is a RingProfiler, so we can safely add the sink
    ringProfiler.AddSink(&traceSink);
  }

  PlatformConfig cfg = {};
  cfg.WindowBackend = WindowBackendKind::Auto;

  Unique<PlatformContext> ctx = PlatformContext::Create(cfg);

  WindowDesc windowDesc;
  windowDesc.Title = "Gecko Platform Example";
  windowDesc.Size = Extent2D{1280, 720};
  windowDesc.Visible = true;
  windowDesc.Resizable = false;
  windowDesc.Mode = WindowMode::Windowed;

  WindowHandle window;
  if (!ctx->CreateWindow(windowDesc, window)) {
    GECKO_ERROR(MAIN_CAT, "Failed to create window\n");
    GECKO_SHUTDOWN();
    return 1;
  }

  bool running = true;
  // Avoid hanging forever in headless/Null-backend runs.
  // Run up to ~10 seconds unless a close is requested.
  u32 frameCount = 0;
  while (running && ctx->IsWindowAlive(window) && frameCount < 600) {
    GECKO_PROF_SCOPE(MAIN_CAT, "Main Loop");
    ctx->PumpEvents();

    WindowEvent ev{};
    while (ctx->PollEvent(ev)) {
      if (ev.Kind == WindowEventKind::CloseRequested) {
        running = false;
        break;
      }
    }

    GECKO_SLEEP_MS(16);
    ++frameCount;
  }

  if (running) {
    // Headless/timeout fallback: request a clean shutdown.
    ctx->RequestClose(window);
  }

  ctx->DestroyWindow(window);

  GECKO_SHUTDOWN();

  return 0;
}
