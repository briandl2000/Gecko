#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gecko/core/services.h>
#include <gecko/core/services/jobs.h>
#include <gecko/core/services/profiler.h>
#include <gecko/runtime/async_trace_profiler_sink.h>
#include <gecko/runtime/ring_profiler.h>
#include <sstream>
#include <string>

using namespace ::gecko;

namespace {

::std::string SlurpFile(const ::std::string& path)
{
  ::std::ifstream f(path, ::std::ios::binary);
  ::std::ostringstream oss;
  oss << f.rdbuf();
  return oss.str();
}

::std::string MakeTempPath(const char* tag)
{
  auto tmp = ::std::filesystem::temp_directory_path() / (::std::string("gecko_trace_test_") + tag + ".json");
  ::std::error_code ec;
  ::std::filesystem::remove(tmp, ec);
  return tmp.string();
}

// Minimal scope: just a RingProfiler. We talk to it directly (no Engine /
// GetProfiler() global state needed for these tests).
struct ProfilerScope
{
  runtime::RingProfiler profiler {1 << 12};
  ProfilerScope()
  {
    profiler.Init();
  }
  ~ProfilerScope()
  {
    profiler.Shutdown();
  }
};

}  // namespace

TEST_CASE("AsyncTraceProfilerSink writes a valid Chrome-trace JSON", "[runtime][profiler][trace]")
{
  const auto path = MakeTempPath("basic");

  {
    runtime::AsyncTraceProfilerSink sink(path.c_str());
    REQUIRE(sink.IsOpen());

    ProfEvent begin {};
    begin.Kind = ProfEventKind::ZoneBegin;
    begin.Name = "TestZone";
    begin.EventLabel = MakeLabel("test.label");
    begin.ThreadId = 42;
    begin.TimestampNs = 1'000'000;
    sink.Write(begin);

    ProfEvent end {};
    end.Kind = ProfEventKind::ZoneEnd;
    end.Name = "TestZone";
    end.EventLabel = MakeLabel("test.label");
    end.ThreadId = 42;
    end.TimestampNs = 2'000'000;
    sink.Write(end);
  }  // sink dtor drains + writes "]}"

  const auto contents = SlurpFile(path);
  REQUIRE(!contents.empty());
  REQUIRE(contents.front() == '{');
  REQUIRE(contents.back() == '}');
  REQUIRE(contents.find("\"traceEvents\":[") != ::std::string::npos);
  REQUIRE(contents.find("TestZone") != ::std::string::npos);
  REQUIRE(contents.find(",,") == ::std::string::npos);  // no double-commas

  ::std::filesystem::remove(path);
}

TEST_CASE("AsyncTraceProfilerSink with a null path is inert", "[runtime][profiler][trace]")
{
  runtime::AsyncTraceProfilerSink sink(nullptr);
  REQUIRE_FALSE(sink.IsOpen());

  ProfEvent ev {};
  ev.Kind = ProfEventKind::FrameMark;
  ev.Name = "Frame";
  ev.ThreadId = 1;
  ev.TimestampNs = 100;
  sink.Write(ev);  // no-op, no crash
  sink.Flush();
}

TEST_CASE("AsyncTraceProfilerSink drains profiler events on dtor", "[runtime][profiler][trace]")
{
  const auto path = MakeTempPath("dtor_drain");

  {
    ProfilerScope scope;
    runtime::AsyncTraceProfilerSink sink(path.c_str());
    REQUIRE(sink.IsOpen());
    sink.RegisterWith(&scope.profiler);

    // Push events directly into the ring; sink dtor must Unregister BEFORE
    // closing the writer or these are lost.
    ProfEvent begin {};
    begin.Kind = ProfEventKind::ZoneBegin;
    begin.Name = "DtorZone";
    begin.EventLabel = MakeLabel("test.dtor_drain");
    begin.ThreadId = 99;
    begin.TimestampNs = 1;
    scope.profiler.Emit(begin);

    ProfEvent end = begin;
    end.Kind = ProfEventKind::ZoneEnd;
    end.TimestampNs = 2;
    scope.profiler.Emit(end);
  }

  const auto contents = SlurpFile(path);
  REQUIRE(contents.find("DtorZone") != ::std::string::npos);
  // Must have both Begin and End for that zone.
  size_t bPos = contents.find("\"ph\":\"B\"");
  size_t ePos = contents.find("\"ph\":\"E\"");
  REQUIRE(bPos != ::std::string::npos);
  REQUIRE(ePos != ::std::string::npos);
  REQUIRE(ePos > bPos);

  ::std::filesystem::remove(path);
}

TEST_CASE("AsyncTraceProfilerSink emits thread_name metadata", "[runtime][profiler][trace]")
{
  const auto path = MakeTempPath("threadname");

  {
    runtime::AsyncTraceProfilerSink sink(path.c_str());
    REQUIRE(sink.IsOpen());

    RegisterThreadProfilerName(7777, "test-worker");

    ProfEvent ev {};
    ev.Kind = ProfEventKind::ZoneBegin;
    ev.Name = "X";
    ev.EventLabel = MakeLabel("test");
    ev.ThreadId = 7777;
    ev.TimestampNs = 10;
    sink.Write(ev);

    ev.Kind = ProfEventKind::ZoneEnd;
    ev.TimestampNs = 20;
    sink.Write(ev);
  }

  const auto contents = SlurpFile(path);
  REQUIRE(contents.find("\"name\":\"thread_name\"") != ::std::string::npos);
  REQUIRE(contents.find("test-worker") != ::std::string::npos);

  ::std::filesystem::remove(path);
  RegisterThreadProfilerName(7777, nullptr);
}

TEST_CASE("AsyncTraceProfilerSink::SetMinLevel filters out lower-level zones", "[runtime][profiler][trace]")
{
  const auto path = MakeTempPath("minlevel");

  {
    runtime::AsyncTraceProfilerSink sink(path.c_str());
    REQUIRE(sink.IsOpen());
    sink.SetMinLevel(ProfLevel::Normal);

    auto emitZone = [&](const char* name, ProfLevel level) {
      ProfEvent begin {};
      begin.Kind = ProfEventKind::ZoneBegin;
      begin.Name = name;
      begin.EventLabel = MakeLabel("test.minlevel");
      begin.ThreadId = 1;
      begin.TimestampNs = 100;
      begin.Level = level;
      sink.Write(begin);

      ProfEvent end = begin;
      end.Kind = ProfEventKind::ZoneEnd;
      end.TimestampNs = 200;
      sink.Write(end);
    };

    emitZone("AlwaysZone", ProfLevel::Always);
    emitZone("NormalZone", ProfLevel::Normal);
    emitZone("DetailedZone", ProfLevel::Detailed);

    // Counter events bypass the level filter.
    ProfEvent counter {};
    counter.Kind = ProfEventKind::Counter;
    counter.Name = "DetailedCounter";
    counter.EventLabel = MakeLabel("test.minlevel");
    counter.ThreadId = 1;
    counter.TimestampNs = 300;
    counter.Level = ProfLevel::Detailed;
    counter.Value = 42;
    sink.Write(counter);
  }

  const auto contents = SlurpFile(path);
  REQUIRE(contents.find("AlwaysZone") != ::std::string::npos);
  REQUIRE(contents.find("NormalZone") != ::std::string::npos);
  // Detailed zones must be dropped because the sink cap is Normal.
  REQUIRE(contents.find("DetailedZone") == ::std::string::npos);
  // Counter must survive the level filter.
  REQUIRE(contents.find("DetailedCounter") != ::std::string::npos);

  ::std::filesystem::remove(path);
}
