#include "gecko/platform/path_view.h"
#include "gecko/platform/platform_io.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

using namespace gecko;
using namespace gecko::platform;

namespace {

// Per-test-case unique scratch dir; cleaned up on scope exit.
struct ScratchDir
{
  ::std::filesystem::path Path;
  ScratchDir()
  {
    auto base = ::std::filesystem::temp_directory_path();
    auto stamp = ::std::chrono::steady_clock::now().time_since_epoch().count();
    Path = base / ("gecko-io-test-" + ::std::to_string(stamp));
    ::std::filesystem::create_directories(Path);
  }
  ~ScratchDir()
  {
    ::std::error_code ec;
    ::std::filesystem::remove_all(Path, ec);
  }
};

::gecko::ConstByteSpan ToBytes(::std::string_view s) noexcept
{
  return {reinterpret_cast<const ::gecko::byte*>(s.data()), s.size()};
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
    REQUIRE(PathView {".bashrc"}.Extension().Empty());
  }
}

// ── Round-trip on real filesystem ──────────────────────────────────────

TEST_CASE("Read/Write/Stat round-trip on real fs", "[platform][io]")
{
  ScratchDir scratch;
  auto target = (scratch.Path / "hello.txt").string();

  SECTION("write then read")
  {
    auto wr = Write(target, ToBytes("hello, world"), WriteMode::Truncate);
    REQUIRE(wr.Ok);
    REQUIRE(wr.BytesWritten == 12);
    REQUIRE(Exists(target));

    auto rr = Read(target);
    REQUIRE(rr.Ok());
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()), rr.Size()};
    REQUIRE(back == "hello, world");
  }

  SECTION("atomic write replaces existing file")
  {
    REQUIRE(AtomicWrite(target, ToBytes("first")));
    REQUIRE(AtomicWrite(target, ToBytes("second")));
    auto rr = Read(target);
    REQUIRE(rr.Ok());
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()), rr.Size()};
    REQUIRE(back == "second");
  }

  SECTION("append mode adds")
  {
    REQUIRE(Write(target, ToBytes("aaa"), WriteMode::Truncate).Ok);
    REQUIRE(Write(target, ToBytes("bbb"), WriteMode::Append).Ok);
    auto rr = Read(target);
    ::std::string back {reinterpret_cast<const char*>(rr.Data().data()), rr.Size()};
    REQUIRE(back == "aaabbb");
  }

  SECTION("stat reports size and is-not-directory")
  {
    REQUIRE(AtomicWrite(target, ToBytes("12345")));
    auto st = Stat(target);
    REQUIRE(st.has_value());
    REQUIRE(st->Size == 5);
    REQUIRE_FALSE(st->IsDirectory);
  }

  SECTION("create dir recursive then remove")
  {
    auto deepPath = (scratch.Path / "a" / "b" / "c").string();
    REQUIRE(CreateDir(deepPath, true));
    auto st = Stat(deepPath);
    REQUIRE(st.has_value());
    REQUIRE(st->IsDirectory);
    REQUIRE(Remove(deepPath));
  }

  SECTION("remove non-existent file fails cleanly")
  {
    REQUIRE_FALSE(Remove((scratch.Path / "nope").string()));
  }

  SECTION("exe path and working dir are non-empty")
  {
    REQUIRE_FALSE(ExePath().empty());
    REQUIRE_FALSE(WorkingDir().empty());
  }

  SECTION("user data dir contains app name")
  {
    auto u = UserDataDir("gecko-test");
    REQUIRE(u.find("gecko-test") != ::std::string::npos);
  }

  SECTION("map round-trip")
  {
    REQUIRE(AtomicWrite(target, ToBytes("mappable bytes")));
    auto m = Map(target);
    REQUIRE(m.Ok());
    REQUIRE(m.Size() == 14);
    ::std::string back {reinterpret_cast<const char*>(m.Data().data()), m.Size()};
    REQUIRE(back == "mappable bytes");
  }

  SECTION("iterate directory finds written files")
  {
    REQUIRE(AtomicWrite((scratch.Path / "a.txt").string(), ToBytes("a")));
    REQUIRE(AtomicWrite((scratch.Path / "b.txt").string(), ToBytes("b")));
    auto it = IterateDir(scratch.Path.string());
    REQUIRE(it.Ok());
    int found = 0;
    while (auto entry = it.Next())
    {
      if (entry->Name == "a.txt" || entry->Name == "b.txt")
        ++found;
    }
    REQUIRE(found == 2);
  }
}

// ── Streaming writer ───────────────────────────────────────────────────

TEST_CASE("OpenWrite streaming round-trip", "[platform][io][stream]")
{
  ScratchDir scratch;
  auto path = (scratch.Path / "streamed.txt").string();

  // Truncate
  {
    auto w = OpenWrite(path, WriteMode::Truncate);
    REQUIRE(w);
    REQUIRE(w->WriteString("alpha"));
    REQUIRE(w->WriteString("\n"));
    REQUIRE(w->Flush());
  }
  REQUIRE(Read(path).Size() == 6);

  // Append
  {
    auto w = OpenWrite(path, WriteMode::Append);
    REQUIRE(w);
    REQUIRE(w->WriteString("beta"));
  }
  REQUIRE(Read(path).Size() == 10);

  // Seek-back overwrite (mirrors crash-safe sink usage)
  {
    auto w = OpenWrite(path, WriteMode::Append);
    REQUIRE(w);
    auto pos = w->Seek(-2, /*fromEnd=*/true);
    REQUIRE(pos == 8);
    REQUIRE(w->WriteString("xx"));
  }
  auto r = Read(path);
  REQUIRE(r.Size() == 10);
  auto sp = r.Data();
  REQUIRE(static_cast<char>(sp[8]) == 'x');
  REQUIRE(static_cast<char>(sp[9]) == 'x');
}
