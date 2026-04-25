#include "test_platform_io.h"

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/platform/path_view.h"
#include "gecko/platform/platform_io.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

using namespace gecko;
using namespace gecko::platform;

namespace {

// Per-process unique scratch dir; cleaned up at scope exit.
struct ScratchDir
{
  ::std::filesystem::path Path;
  ScratchDir()
  {
    auto base = ::std::filesystem::temp_directory_path();
    auto pid = ::std::chrono::steady_clock::now().time_since_epoch().count();
    Path = base / ("gecko-io-test-" + ::std::to_string(pid));
    ::std::filesystem::create_directories(Path);
  }
  ~ScratchDir()
  {
    ::std::error_code ec;
    ::std::filesystem::remove_all(Path, ec);
  }
};

struct PlatformScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::CoreServicesModule runtimeMod;
  PlatformModule platformMod;
  ::std::optional<::gecko::Engine> engine;

  PlatformScope() : runtimeMod(jobs, profiler, logger, events)
  {
    REQUIRE(SetAllocator(&alloc));
    engine = ::gecko::Engine::Create({&runtimeMod, &platformMod});
    REQUIRE(engine.has_value());
  }
  ~PlatformScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

::std::span<const ::std::byte> ToBytes(::std::string_view s) noexcept
{
  return {reinterpret_cast<const ::std::byte*>(s.data()), s.size()};
}

}  // namespace

// ── PathView ───────────────────────────────────────────────────────────

TEST_CASE("PathView decomposition", "[platform][io][path]")
{
  SECTION("absolute / relative")
  {
    REQUIRE(PathView {"/etc/hosts"}.IsAbsolute());
    REQUIRE_FALSE(PathView {"relative/path"}.IsAbsolute());
    REQUIRE(PathView {"C:/Users/Test"}.IsAbsolute());
    REQUIRE_FALSE(PathView {"C:relative"}.IsAbsolute());
  }
  SECTION("filename and parent")
  {
    REQUIRE(PathView {"/a/b/c.txt"}.Filename().View() == "c.txt");
    REQUIRE(PathView {"/a/b/c.txt"}.ParentDir().View() == "/a/b");
    REQUIRE(PathView {"plain.txt"}.Filename().View() == "plain.txt");
    REQUIRE(PathView {"plain.txt"}.ParentDir().Empty());
  }
  SECTION("stem and extension")
  {
    REQUIRE(PathView {"foo.tar.gz"}.Stem().View() == "foo.tar");
    REQUIRE(PathView {"foo.tar.gz"}.Extension().View() == ".gz");
    REQUIRE(PathView {"noext"}.Stem().View() == "noext");
    REQUIRE(PathView {"noext"}.Extension().Empty());
    // Dotfile: ".bashrc" has no extension by convention.
    REQUIRE(PathView {".bashrc"}.Extension().Empty());
  }
}

// ── Default backend fallback ───────────────────────────────────────────

TEST_CASE("GetPlatformIO returns a working backend before engine boots",
          "[platform][io]")
{
  // When no PlatformModule has booted, GetPlatformIO falls back to the
  // native backend so IO callers (e.g. FileLogSink in apps that don't
  // include PlatformModule) still work. Nonexistent path queries should
  // be cleanly false.
  IPlatformIO* io = GetPlatformIO();
  REQUIRE(io != nullptr);
  REQUIRE_FALSE(io->Exists("/this/path/does/not/exist/12345"));
  REQUIRE_FALSE(io->Stat("/this/path/does/not/exist/12345").has_value());
  REQUIRE_FALSE(io->Read("/this/path/does/not/exist/12345").Ok());
}

// ── End-to-end against real backend ───────────────────────────────────

TEST_CASE("PlatformModule publishes IPlatformIO; round-trip on real fs",
          "[platform][io]")
{
  PlatformScope scope;
  ScratchDir scratch;

  IPlatformIO* io = GetPlatformIO();
  REQUIRE(io != nullptr);

  auto target = (scratch.Path / "hello.txt").string();

  SECTION("write then read")
  {
    auto wr = io->Write(target, ToBytes("hello, world"), WriteMode::Truncate);
    REQUIRE(wr.Ok);
    REQUIRE(wr.BytesWritten == 12);
    REQUIRE(io->Exists(target));

    auto rr = io->Read(target);
    REQUIRE(rr.Ok());
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()),
                        rr.Size()};
    REQUIRE(back == "hello, world");
  }

  SECTION("atomic write replaces existing file")
  {
    REQUIRE(io->AtomicWrite(target, ToBytes("first")));
    REQUIRE(io->AtomicWrite(target, ToBytes("second")));
    auto rr = io->Read(target);
    REQUIRE(rr.Ok());
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()),
                        rr.Size()};
    REQUIRE(back == "second");
  }

  SECTION("append mode adds")
  {
    REQUIRE(io->Write(target, ToBytes("aaa"), WriteMode::Truncate).Ok);
    REQUIRE(io->Write(target, ToBytes("bbb"), WriteMode::Append).Ok);
    auto rr = io->Read(target);
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()),
                        rr.Size()};
    REQUIRE(back == "aaabbb");
  }

  SECTION("stat reports size and is-not-directory")
  {
    REQUIRE(io->AtomicWrite(target, ToBytes("12345")));
    auto st = io->Stat(target);
    REQUIRE(st.has_value());
    REQUIRE(st->Size == 5);
    REQUIRE_FALSE(st->IsDirectory);
  }

  SECTION("create dir recursive then remove")
  {
    auto deepPath = (scratch.Path / "a" / "b" / "c").string();
    REQUIRE(io->CreateDir(deepPath, true));
    auto st = io->Stat(deepPath);
    REQUIRE(st.has_value());
    REQUIRE(st->IsDirectory);
    REQUIRE(io->Remove(deepPath));
  }

  SECTION("remove non-existent file fails cleanly")
  {
    REQUIRE_FALSE(io->Remove((scratch.Path / "nope").string()));
  }

  SECTION("exe path and working dir are non-empty")
  {
    REQUIRE_FALSE(io->ExePath().empty());
    REQUIRE_FALSE(io->WorkingDir().empty());
  }

  SECTION("user data dir contains app name")
  {
    auto u = io->UserDataDir("gecko-test");
    REQUIRE(u.find("gecko-test") != ::std::string::npos);
  }

  SECTION("map round-trip")
  {
    REQUIRE(io->AtomicWrite(target, ToBytes("mappable bytes")));
    auto m = io->Map(target);
    REQUIRE(m.Ok());
    REQUIRE(m.Size() == 14);
    ::std::string back {reinterpret_cast<const char*>(m.Data().data()),
                        m.Size()};
    REQUIRE(back == "mappable bytes");
  }

  SECTION("iterate directory finds written files")
  {
    REQUIRE(io->AtomicWrite((scratch.Path / "a.txt").string(), ToBytes("a")));
    REQUIRE(io->AtomicWrite((scratch.Path / "b.txt").string(), ToBytes("b")));
    auto it = io->IterateDir(scratch.Path.string());
    REQUIRE(it.Ok());
    int found = 0;
    DirEntry entry;
    while (it.Next(entry))
    {
      if (entry.Name == "a.txt" || entry.Name == "b.txt")
        ++found;
    }
    REQUIRE(found == 2);
  }
}

// ── TestPlatformIO smoke ───────────────────────────────────────────────

TEST_CASE("TestPlatformIO basic round-trip", "[platform][io][fake]")
{
  ::gecko::test::TestPlatformIO io;

  REQUIRE_FALSE(io.Exists("/x/file.txt"));
  REQUIRE(io.AtomicWrite("/x/file.txt", ToBytes("hi")));
  REQUIRE(io.Exists("/x/file.txt"));
  REQUIRE(io.Exists("/x"));  // parent dir was auto-created

  auto rr = io.Read("/x/file.txt");
  REQUIRE(rr.Ok());
  REQUIRE(rr.Size() == 2);

  REQUIRE(io.Remove("/x/file.txt"));
  REQUIRE_FALSE(io.Exists("/x/file.txt"));
}

// ── Streaming writer tests ─────────────────────────────────────────────

TEST_CASE("OpenWrite streaming on real fs", "[platform][io][stream]")
{
  PlatformScope scope;
  auto* io = ::gecko::platform::GetPlatformIO();

  ::std::string root = io->WorkingDir() + "/.test_io_stream";
  REQUIRE(io->CreateDir(root, /*recursive=*/true));
  ::std::string path = root + "/streamed.txt";

  // Truncate write
  {
    auto w = io->OpenWrite(path, ::gecko::platform::WriteMode::Truncate);
    REQUIRE(w);
    REQUIRE(w->WriteString("alpha"));
    REQUIRE(w->WriteString("\n"));
    REQUIRE(w->Flush());
  }
  auto r1 = io->Read(path);
  REQUIRE(r1.Ok());
  REQUIRE(r1.Size() == 6);

  // Append
  {
    auto w = io->OpenWrite(path, ::gecko::platform::WriteMode::Append);
    REQUIRE(w);
    REQUIRE(w->WriteString("beta"));
  }
  auto r2 = io->Read(path);
  REQUIRE(r2.Size() == 10);

  // Seek-back overwrite (as crash-safe sink does)
  {
    auto w = io->OpenWrite(path, ::gecko::platform::WriteMode::Append);
    REQUIRE(w);
    auto pos = w->Seek(-2, /*fromEnd=*/true);
    REQUIRE(pos == 8);
    REQUIRE(w->WriteString("xx"));
  }
  auto r3 = io->Read(path);
  REQUIRE(r3.Size() == 10);
  // Last two bytes should now be "xx".
  auto sp = r3.Data();
  REQUIRE(static_cast<char>(sp[8]) == 'x');
  REQUIRE(static_cast<char>(sp[9]) == 'x');

  REQUIRE(io->Remove(path));
  REQUIRE(io->Remove(root));
}

TEST_CASE("TestPlatformIO streaming writer", "[platform][io][fake]")
{
  ::gecko::test::TestPlatformIO io;

  {
    auto w = io.OpenWrite("/log/a.txt", ::gecko::platform::WriteMode::Truncate);
    REQUIRE(w);
    REQUIRE(w->WriteString("hello"));
    REQUIRE(w->Tell() == 5);
  }
  auto r = io.Read("/log/a.txt");
  REQUIRE(r.Size() == 5);

  {
    auto w = io.OpenWrite("/log/a.txt", ::gecko::platform::WriteMode::Append);
    REQUIRE(w);
    REQUIRE(w->Seek(-1, true) == 4);  // seek before final 'o'
    REQUIRE(w->WriteString("p!"));
  }
  auto r2 = io.Read("/log/a.txt");
  REQUIRE(r2.Size() == 6);
  REQUIRE(static_cast<char>(r2.Data()[4]) == 'p');
  REQUIRE(static_cast<char>(r2.Data()[5]) == '!');
}
