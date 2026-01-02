#include <cstdlib>
#include <cstring>
#include <string_view>

#include <gecko/core/boot.h>
#include <gecko/core/log.h>
#include <gecko/core/memory.h>
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

const auto APP_CAT = MakeCategory("App");

struct AppConfig {
  const char *title = "Gecko App";
  bool windowed = true;
  u32 maxFrames = 0; // 0 = run until close
  WindowBackendKind backend = WindowBackendKind::Auto;
};

static void PrintUsage(const char *exe) {
  std::fprintf(
      stderr,
      "Usage: %s [options]\n\n"
      "Options:\n"
      "  --no-window           Run without creating a window\n"
      "  --frames=N            Run N frames then exit (windowed only)\n"
      "  --title=TEXT          Window title (windowed only)\n"
      "  --backend=auto|null|xlib  Window backend (Linux only supports xlib/auto today)\n"
      "  --help                Show this help\n",
      exe ? exe : "app_skeleton");
}

static bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

static bool ParseU32(std::string_view s, u32 &out) {
  if (s.empty())
    return false;
  u64 value = 0;
  for (char c : s) {
    if (c < '0' || c > '9')
      return false;
    value = value * 10 + static_cast<u64>(c - '0');
    if (value > 0xFFFFFFFFu)
      return false;
  }
  out = static_cast<u32>(value);
  return true;
}

static bool ParseArgs(int argc, char **argv, AppConfig &cfg) {
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i] ? argv[i] : "";

    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return false;
    }

    if (arg == "--no-window") {
      cfg.windowed = false;
      continue;
    }

    if (StartsWith(arg, "--frames=")) {
      std::string_view value = arg.substr(std::strlen("--frames="));
      if (!ParseU32(value, cfg.maxFrames)) {
        std::fprintf(stderr, "Invalid --frames value: %.*s\n",
                     static_cast<int>(value.size()), value.data());
        return false;
      }
      continue;
    }

    if (StartsWith(arg, "--title=")) {
      // argv storage stays valid for the life of the process.
      cfg.title = argv[i] + std::strlen("--title=");
      continue;
    }

    if (StartsWith(arg, "--backend=")) {
      std::string_view value = arg.substr(std::strlen("--backend="));
      if (value == "auto") {
        cfg.backend = WindowBackendKind::Auto;
      } else if (value == "null") {
        cfg.backend = WindowBackendKind::Null;
      } else if (value == "xlib") {
        cfg.backend = WindowBackendKind::Xlib;
      } else {
        std::fprintf(stderr, "Invalid --backend value: %.*s\n",
                     static_cast<int>(value.size()), value.data());
        return false;
      }
      continue;
    }

    std::fprintf(stderr, "Unknown arg: %.*s\n", static_cast<int>(arg.size()),
                 arg.data());
    PrintUsage(argv[0]);
    return false;
  }

  return true;
}

static Services CreateServices(runtime::TrackingAllocator &trackingAlloc,
                              runtime::ThreadPoolJobSystem &jobSystem,
                              runtime::RingProfiler &ringProfiler,
                              runtime::RingLogger &ringLogger) {
  Services services{};
  services.Allocator = &trackingAlloc;
  services.JobSystem = &jobSystem;
  services.Profiler = &ringProfiler;
  services.Logger = &ringLogger;
  return services;
}

static int AppMain(int argc, char **argv) {
  AppConfig cfg{};
  if (!ParseArgs(argc, argv, cfg)) {
    // ParseArgs prints usage on errors/help.
    return 2;
  }

  // 1) Choose concrete implementations for core services.
  SystemAllocator systemAlloc;
  runtime::TrackingAllocator trackingAlloc(&systemAlloc);

  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  runtime::RingProfiler ringProfiler(1 << 16);
  runtime::RingLogger ringLogger(1024);

  // 2) Install services (Allocator -> JobSystem -> Profiler -> Logger).
  GECKO_BOOT((CreateServices(trackingAlloc, jobSystem, ringProfiler, ringLogger)));

  // 3) Configure sinks AFTER services are installed.
  runtime::ConsoleLogSink consoleSink;
  runtime::FileLogSink fileSink("log.txt");
  if (auto *logger = GetLogger()) {
    logger->AddSink(&consoleSink);
    logger->AddSink(&fileSink);
    logger->SetLevel(LogLevel::Info);
  }

  gecko::LogVersion(APP_CAT);

  runtime::TraceFileSink traceSink("gecko_trace.json");
  if (traceSink.IsOpen()) {
    ringProfiler.AddSink(&traceSink);
  } else {
    GECKO_WARN(APP_CAT, "Failed to open trace file sink");
  }

  // 4) Run either headless or windowed.
  int result = 0;

  if (!cfg.windowed) {
    GECKO_INFO(APP_CAT, "Running headless");
    // Your headless work goes here.
    GECKO_SLEEP_MS(10);
  } else {
    PlatformConfig platformCfg{};
    platformCfg.WindowBackend = cfg.backend;

    Unique<PlatformContext> ctx = PlatformContext::Create(platformCfg);
    if (!ctx) {
      GECKO_ERROR(APP_CAT, "Failed to create PlatformContext");
      GECKO_SHUTDOWN();
      return 1;
    }

    WindowDesc windowDesc{};
    windowDesc.Title = cfg.title;
    windowDesc.Size = Extent2D{1280, 720};
    windowDesc.Visible = true;
    windowDesc.Resizable = true;

    WindowHandle window{};
    if (!ctx->CreateWindow(windowDesc, window)) {
      GECKO_ERROR(APP_CAT, "Failed to create window");
      GECKO_SHUTDOWN();
      return 1;
    }

    bool running = true;
    u32 frames = 0;

    while (running && ctx->IsWindowAlive(window)) {
      GECKO_PROF_SCOPE(APP_CAT, "Frame");

      ctx->PumpEvents();
      WindowEvent ev{};
      while (ctx->PollEvent(ev)) {
        if (ev.Kind == WindowEventKind::CloseRequested) {
          running = false;
          break;
        }
      }

      // Your update/render work goes here.
      GECKO_SLEEP_MS(16);

      ++frames;
      if (cfg.maxFrames != 0 && frames >= cfg.maxFrames) {
        running = false;
      }
    }

    ctx->DestroyWindow(window);
  }

  // 5) Shutdown services in reverse order.
  GECKO_SHUTDOWN();
  return result;
}

#if defined(_WIN32)
// On Windows, you typically choose ONE of:
// - main(int,char**) for a console subsystem app
// - wmain(int,wchar_t**) to preserve Unicode arguments
// - WinMain/wWinMain for a GUI subsystem app
// Gecko does not currently provide an entrypoint abstraction; route into AppMain.
int main(int argc, char **argv) { return AppMain(argc, argv); }
#else
int main(int argc, char **argv) { return AppMain(argc, argv); }
#endif
