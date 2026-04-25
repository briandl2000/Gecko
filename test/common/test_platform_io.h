#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/types.h"
#include "gecko/platform/platform_io.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace gecko::test {

// In-memory IPlatformIO implementation. Stores files as
// (path -> bytes). Directories are tracked as a separate set so empty
// dirs survive removal of their files.
//
// Construct one, hand it to your subject under test (or publish it via
// PlatformModule equivalents in integration tests), then assert on the
// observable state via Files() / Dirs().
class TestPlatformIO final : public ::gecko::platform::IPlatformIO
{
public:
  TestPlatformIO() = default;

  // Direct-access helpers for test assertions ─────────────────────────

  using FileMap = ::std::map<::std::string, ::std::vector<::std::byte>>;
  using DirSet = ::std::map<::std::string, bool>;  // value unused

  [[nodiscard]] const FileMap& Files() const noexcept
  {
    return m_Files;
  }
  [[nodiscard]] const DirSet& Dirs() const noexcept
  {
    return m_Dirs;
  }

  // Pre-populate a file. Useful in test setup to stage inputs.
  void PutFile(::std::string_view path,
               ::std::span<const ::std::byte> data) noexcept
  {
    m_Files[::std::string {path}].assign(data.begin(), data.end());
    EnsureParents(path);
  }

  // IPlatformIO surface ───────────────────────────────────────────────

  bool Exists(::gecko::platform::PathView path) noexcept override
  {
    auto key = ::std::string {path.View()};
    return m_Files.contains(key) || m_Dirs.contains(key);
  }

  ::std::optional<::gecko::platform::FileStat> Stat(
      ::gecko::platform::PathView path) noexcept override
  {
    auto key = ::std::string {path.View()};
    auto it = m_Files.find(key);
    if (it != m_Files.end())
    {
      ::gecko::platform::FileStat fs {};
      fs.Size = it->second.size();
      fs.MTimeEpoch = 0;
      fs.IsDirectory = false;
      return fs;
    }
    if (m_Dirs.contains(key))
    {
      ::gecko::platform::FileStat fs {};
      fs.IsDirectory = true;
      return fs;
    }
    return ::std::nullopt;
  }

  ::gecko::platform::ReadResult Read(
      ::gecko::platform::PathView path) noexcept override
  {
    auto it = m_Files.find(::std::string {path.View()});
    if (it == m_Files.end())
      return {};
    return ::gecko::platform::ReadResult {it->second};
  }

  ::gecko::platform::MappedFile Map(
      ::gecko::platform::PathView /*path*/) noexcept override
  {
    // Test fake: not supported. Use Read() instead.
    return {};
  }

  ::gecko::platform::WriteResult Write(
      ::gecko::platform::PathView path, ::std::span<const ::std::byte> data,
      ::gecko::platform::WriteMode mode) noexcept override
  {
    auto key = ::std::string {path.View()};
    auto& slot = m_Files[key];
    if (mode == ::gecko::platform::WriteMode::Truncate)
      slot.clear();
    slot.insert(slot.end(), data.begin(), data.end());
    EnsureParents(path);
    return ::gecko::platform::WriteResult {
        .Ok = true, .BytesWritten = static_cast<::gecko::u64>(data.size())};
  }

  bool AtomicWrite(::gecko::platform::PathView path,
                   ::std::span<const ::std::byte> data) noexcept override
  {
    auto key = ::std::string {path.View()};
    m_Files[key].assign(data.begin(), data.end());
    EnsureParents(path);
    return true;
  }

  ::gecko::Unique<::gecko::platform::IFileWriter> OpenWrite(
      ::gecko::platform::PathView path,
      ::gecko::platform::WriteMode mode) noexcept override
  {
    auto key = ::std::string {path.View()};
    if (mode == ::gecko::platform::WriteMode::Truncate)
      m_Files[key].clear();
    else
      (void)m_Files[key];  // ensure entry exists for append
    EnsureParents(path);
    return ::gecko::CreateUnique<TestFileWriter>(&m_Files[key]);
  }

  bool CreateDir(::gecko::platform::PathView path,
                 bool recursive) noexcept override
  {
    auto sv = path.View();
    if (sv.empty())
      return false;
    if (recursive)
    {
      EnsureParents(path);
      m_Dirs[::std::string {sv}] = true;
      return true;
    }
    // Non-recursive: parent must already exist (or path is a single
    // component with no '/').
    auto pos = sv.rfind('/');
    if (pos != ::std::string_view::npos && pos != 0)
    {
      auto parent = ::std::string {sv.substr(0, pos)};
      if (!m_Dirs.contains(parent))
        return false;
    }
    m_Dirs[::std::string {sv}] = true;
    return true;
  }

  bool Remove(::gecko::platform::PathView path) noexcept override
  {
    auto key = ::std::string {path.View()};
    if (auto it = m_Files.find(key); it != m_Files.end())
    {
      m_Files.erase(it);
      return true;
    }
    if (auto it = m_Dirs.find(key); it != m_Dirs.end())
    {
      m_Dirs.erase(it);
      return true;
    }
    return false;
  }

  ::gecko::platform::DirIter IterateDir(
      ::gecko::platform::PathView /*path*/) noexcept override
  {
    // Not implemented for test fake. Tests that need iteration can
    // inspect Files() / Dirs() directly.
    return {};
  }

  ::std::string ExePath() noexcept override
  {
    return "/test/bin/test_executable";
  }
  ::std::string WorkingDir() noexcept override
  {
    return "/test/wd";
  }
  ::std::string UserDataDir(::std::string_view appName) noexcept override
  {
    ::std::string s = "/test/userdata";
    if (!appName.empty())
    {
      s.push_back('/');
      s.append(appName);
    }
    return s;
  }

private:
  class TestFileWriter final : public ::gecko::platform::IFileWriter
  {
  public:
    explicit TestFileWriter(::std::vector<::std::byte>* slot) noexcept
        : m_Slot(slot)
    {}

    bool Write(::std::span<const ::std::byte> data) noexcept override
    {
      if (!m_Slot)
        return false;
      auto pos = static_cast<::std::size_t>(m_Pos);
      if (pos + data.size() > m_Slot->size())
        m_Slot->resize(pos + data.size());
      ::std::copy(data.begin(), data.end(), m_Slot->begin() + pos);
      m_Pos += data.size();
      return true;
    }

    bool Flush() noexcept override
    {
      return m_Slot != nullptr;
    }

    ::gecko::u64 Seek(::gecko::i64 offset, bool fromEnd) noexcept override
    {
      if (!m_Slot)
        return static_cast<::gecko::u64>(-1);
      ::gecko::i64 base =
          fromEnd ? static_cast<::gecko::i64>(m_Slot->size()) : 0;
      ::gecko::i64 absolute = base + offset;
      if (absolute < 0)
        return static_cast<::gecko::u64>(-1);
      m_Pos = static_cast<::gecko::u64>(absolute);
      return m_Pos;
    }

    ::gecko::u64 Tell() noexcept override
    {
      return m_Pos;
    }

  private:
    ::std::vector<::std::byte>* m_Slot {nullptr};
    ::gecko::u64 m_Pos {0};
  };

  void EnsureParents(::gecko::platform::PathView path) noexcept
  {
    auto sv = path.View();
    for (::std::size_t i = 1; i < sv.size(); ++i)
    {
      if (sv[i] == '/')
        m_Dirs[::std::string {sv.substr(0, i)}] = true;
    }
  }

  FileMap m_Files;
  DirSet m_Dirs;
};

}  // namespace gecko::test
