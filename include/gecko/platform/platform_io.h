#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/types.h"
#include "gecko/platform/path_view.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Filesystem and process-info utilities.
//
// These are stateless namespace functions backed by the OS. The Linux
// and Win32 implementations live in src/platform/{linux,win32}/ and are
// selected at compile time. There is no IPlatformIO service: per the
// API shaping matrix in copilot_context/MODULE_API_SHAPING.md a
// stateless API is exposed as namespace functions, not as a service.
//
// Path arguments are forward-slash PathView values; the Win32 backend
// converts to native UTF-16 + backslashes at the syscall boundary.

namespace gecko::platform {

// -- Plain data ------------------------------------------------------

struct FileStat
{
  ::gecko::u64 Size {0};
  ::gecko::i64 MTimeEpoch {-1};  // seconds since unix epoch; -1 = unknown
  bool IsDirectory {false};
};

enum class WriteMode : ::gecko::u8
{
  Truncate,
  Append,
};

struct WriteResult
{
  bool Ok {false};
  ::gecko::u64 BytesWritten {0};

  explicit operator bool() const noexcept
  {
    return Ok;
  }
};

struct DirEntry
{
  ::std::string Name {};
  bool IsDirectory {false};
};

// -- Move-only owning result types -----------------------------------

class GECKO_API ReadResult
{
public:
  ReadResult() noexcept = default;
  explicit ReadResult(::std::vector<::std::byte> bytes) noexcept;

  ReadResult(const ReadResult&) = delete;
  ReadResult& operator=(const ReadResult&) = delete;
  ReadResult(ReadResult&&) noexcept;
  ReadResult& operator=(ReadResult&&) noexcept;
  ~ReadResult() noexcept;

  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Ok;
  }
  explicit operator bool() const noexcept
  {
    return m_Ok;
  }
  [[nodiscard]] ::std::span<const ::std::byte> Data() const noexcept
  {
    return {m_Bytes.data(), m_Bytes.size()};
  }
  [[nodiscard]] ::std::size_t Size() const noexcept
  {
    return m_Bytes.size();
  }
  [[nodiscard]] ::std::vector<::std::byte> Take() noexcept;

private:
  ::std::vector<::std::byte> m_Bytes {};
  bool m_Ok {false};
};

/// Read-only memory-mapped file. Owns the mapping; move-only.
class GECKO_API MappedFile
{
public:
  using Deleter = void (*)(void* handle) noexcept;

  MappedFile() noexcept = default;
  MappedFile(const ::std::byte* data, ::std::size_t size, void* handle, Deleter deleter) noexcept;

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;
  ~MappedFile() noexcept;

  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return Ok();
  }
  [[nodiscard]] ::std::span<const ::std::byte> Data() const noexcept
  {
    return {m_Data, m_Size};
  }
  [[nodiscard]] ::std::size_t Size() const noexcept
  {
    return m_Size;
  }

private:
  void Reset() noexcept;

  const ::std::byte* m_Data {nullptr};
  ::std::size_t m_Size {0};
  void* m_Handle {nullptr};
  Deleter m_Deleter {nullptr};
};

/// Forward-iterator-style directory enumeration handle. Move-only.
class GECKO_API DirIter
{
public:
  using NextFn = bool (*)(void* handle, DirEntry* out) noexcept;
  using CloseFn = void (*)(void* handle) noexcept;

  DirIter() noexcept = default;
  DirIter(void* handle, NextFn nextFn, CloseFn closeFn) noexcept;

  DirIter(const DirIter&) = delete;
  DirIter& operator=(const DirIter&) = delete;
  DirIter(DirIter&& other) noexcept;
  DirIter& operator=(DirIter&& other) noexcept;
  ~DirIter() noexcept;

  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Handle != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return Ok();
  }

  [[nodiscard]] ::std::optional<DirEntry> Next() noexcept;
  void Close() noexcept;

private:
  void* m_Handle {nullptr};
  NextFn m_Next {nullptr};
  CloseFn m_Close {nullptr};
};

// Streaming write handle. Created by OpenWrite; owned by the caller.
// Destruction closes the file. Not a service: each instance is an
// opaque RAII handle around an OS file descriptor.
class GECKO_API FileWriter
{
public:
  virtual ~FileWriter() = default;

  // Write the full span; returns false on partial-write or error.
  virtual bool Write(::std::span<const ::std::byte> data) noexcept = 0;

  // Convenience for text payloads.
  bool WriteString(::std::string_view text) noexcept;

  // Flush buffered bytes to the OS (no fsync).
  virtual bool Flush() noexcept = 0;

  // Absolute seek. fromEnd=true makes offset relative to EOF.
  // Returns the resulting position, or u64(-1) on failure.
  virtual ::gecko::u64 Seek(::gecko::i64 offset, bool fromEnd) noexcept = 0;

  // Current absolute position, or u64(-1) on failure.
  [[nodiscard]] virtual ::gecko::u64 Tell() noexcept = 0;
};

// -- Public API: stateless free functions ----------------------------
//
// All paths are forward-slash PathView. All functions are noexcept and
// return false / empty / nullopt on error.

// Existence and metadata
[[nodiscard]] GECKO_API bool Exists(PathView path) noexcept;
[[nodiscard]] GECKO_API ::std::optional<FileStat> Stat(PathView path) noexcept;

// Whole-file read
[[nodiscard]] GECKO_API ReadResult Read(PathView path) noexcept;

// Memory map (read-only)
[[nodiscard]] GECKO_API MappedFile Map(PathView path) noexcept;

// Whole-file write (truncate or append)
GECKO_API WriteResult Write(PathView path, ::std::span<const ::std::byte> data, WriteMode mode) noexcept;

// Atomic replace via tmp+rename (with fsync on Linux, MOVEFILE_REPLACE
// on Win32). Returns false on any step failure.
[[nodiscard]] GECKO_API bool AtomicWrite(PathView path, ::std::span<const ::std::byte> data) noexcept;

// Streaming write handle. Append mode positions cursor at EOF but does
// NOT enable POSIX O_APPEND; subsequent Seek+Write may overwrite
// anywhere. Returns null on failure.
[[nodiscard]] GECKO_API ::gecko::Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept;

// Directory operations
GECKO_API bool CreateDir(PathView path, bool recursive) noexcept;
GECKO_API bool Remove(PathView path) noexcept;
[[nodiscard]] GECKO_API DirIter IterateDir(PathView path) noexcept;

// Well-known paths (forward-slash, owning string; empty on failure).
// ExePath includes the executable name; WorkingDir does not.
[[nodiscard]] GECKO_API ::std::string ExePath() noexcept;
[[nodiscard]] GECKO_API ::std::string WorkingDir() noexcept;
[[nodiscard]] GECKO_API ::std::string UserDataDir(::std::string_view appName) noexcept;

}  // namespace gecko::platform
