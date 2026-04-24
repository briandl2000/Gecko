#include "gecko/core/services.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("Services are not installed initially", "[core][services]")
{
  REQUIRE_FALSE(IsServicesInstalled());
}

TEST_CASE("Allocator returns default before any SetAllocator",
          "[core][services]")
{
  REQUIRE_FALSE(IsServicesInstalled());
  IAllocator& alloc = Allocator();
  // Reference-returning API: we just exercise it.
  void* p = alloc.Alloc(16, alignof(::std::max_align_t));
  REQUIRE(p != nullptr);
  alloc.Free(p);
}

TEST_CASE("Install and uninstall with null services", "[core][services]")
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  NullModuleRegistry modules;
  NullEventBus events;

  REQUIRE(SetAllocator(&alloc));
  jobs.Init();
  profiler.Init();
  logger.Init();
  REQUIRE(modules.Init());
  events.Init();

  Services svc {
      .JobSystem = &jobs,
      .Profiler = &profiler,
      .Logger = &logger,
      .Modules = &modules,
      .EventBus = &events,
  };

  REQUIRE(InstallServices(svc));
  REQUIRE(IsServicesInstalled());
  REQUIRE(ValidateServices(false));

  REQUIRE(&Allocator() == &alloc);
  REQUIRE(GetJobSystem() == &jobs);
  REQUIRE(GetProfiler() == &profiler);
  REQUIRE(GetLogger() == &logger);
  REQUIRE(GetModules() == &modules);
  REQUIRE(GetEventBus() == &events);

  UninstallServices();
  ResetAllocator();
  REQUIRE_FALSE(IsServicesInstalled());
}

TEST_CASE("NullJobSystem runs inline", "[core][services]")
{
  NullJobSystem jobs;
  jobs.Init();

  bool ran = false;
  JobHandle h = jobs.Submit([&ran]() { ran = true; });
  REQUIRE(ran);
  REQUIRE(jobs.IsComplete(h));

  jobs.Shutdown();
}

TEST_CASE("NullProfiler no-ops", "[core][services]")
{
  NullProfiler profiler;
  REQUIRE(profiler.Init());
  REQUIRE(profiler.NowNs() == 0);
  REQUIRE(profiler.IsLevelEnabled(ProfLevel::Normal));

  ProfEvent ev {};
  profiler.Emit(ev);

  profiler.Shutdown();
}

TEST_CASE("NullLogger no-ops", "[core][services]")
{
  NullLogger logger;
  REQUIRE(logger.Init());
  logger.Log(LogLevel::Info, MakeLabel("test"), "hello %s", "world");
  logger.Flush();
  logger.Shutdown();
}

TEST_CASE("JobHandle validity", "[core][services]")
{
  JobHandle h;
  REQUIRE_FALSE(h.IsValid());
  REQUIRE(h.Id == 0);

  JobHandle h2 {42};
  REQUIRE(h2.IsValid());
  REQUIRE(h != h2);

  h2.Reset();
  REQUIRE_FALSE(h2.IsValid());
  REQUIRE(h == h2);
}

TEST_CASE("LevelName for all log levels", "[core][services]")
{
  REQUIRE(::std::string(LevelName(LogLevel::Trace)) == "TRACE");
  REQUIRE(::std::string(LevelName(LogLevel::Debug)) == "DEBUG");
  REQUIRE(::std::string(LevelName(LogLevel::Info)) == "INFO");
  REQUIRE(::std::string(LevelName(LogLevel::Warn)) == "WARN");
  REQUIRE(::std::string(LevelName(LogLevel::Error)) == "ERROR");
  REQUIRE(::std::string(LevelName(LogLevel::Fatal)) == "FATAL");
}
