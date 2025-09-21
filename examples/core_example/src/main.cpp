#include <thread>
#include <atomic>
#include <cstdio>

#include "gecko/core/category.h"
#include "gecko/core/services.h"
#include "gecko/core/profiler.h"

// runtime impls
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/trace_writer.h"

namespace categories {
  inline constexpr auto example = gecko::MakeCategory("core_example");
  inline constexpr auto work = gecko::MakeCategory("work");
}

int worker_func(int id, int frames) {
  GECKO_PROF_FUNC(categories::example);

  for (int i = 0; i < frames; ++i) {
    GECKO_PROF_ZONE(categories::work, "WorkerZone");

    std::this_thread::sleep_for(std::chrono::milliseconds(3));
  }
  return id;
}

int main(int argc, char** argv) {
  const char* trace_path = (argc > 1) ? argv[1] : "trace.json";

  gecko::runtime::RingProfiler prof(1u << 18);
  gecko::InstallServices({ .profiler = &prof });

  gecko::runtime::TraceWriter tw;
  if (!tw.Open(trace_path)) {
    std::fprintf(stderr, "Failed to open %s\n", trace_path);
    return 1;
  }

  std::atomic<bool> run{true};
  // Consumer thread: drain events to JSON
  std::thread drain([&]{
    gecko::ProfEvent ev{};
    while (run.load(std::memory_order_relaxed)) {
      while (prof.TryPop(ev)) tw.Write(ev);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // final drain
    while (prof.TryPop(ev)) tw.Write(ev);
  });

  // Second worker thread
  std::thread worker([&]{
    GECKO_PROF_ZONE(categories::work, "WorkerThread");
    worker_func(42, 120);
  });

  // Emit some frames and zones
  for (int f=0; f<120; ++f) {
    GECKO_PROF_FRAME(categories::example, "frame");
    {
      GECKO_PROF_ZONE(categories::example, "Tick");
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
      GECKO_PROF_ZONE(categories::work, "WorkA");
      {
        GECKO_PROF_ZONE(categories::work, "WorkB");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    GECKO_PROF_COUNTER(categories::example, "frame_idx", f);
  }

  run = false;
  drain.join();
  worker.join();
  tw.Close();

  std::puts("Wrote trace.json (open in chrome://tracing)");
  return 0;
}
