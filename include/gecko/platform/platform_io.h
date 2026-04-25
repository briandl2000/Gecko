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

namespace gecko::platform {

// Result of a Stat() call. All fields are best-effort; backends that
// cannot determine MTimeEpoch return -1.
struct FileStat
{
  ::gecko::u64 Size {0};
  ::gecko::i64 MTimeEpoch {-1};  // seconds since unix epoch; -1 = unknown
  bool IsDirectory {false};
};

enum class WriteMode : ::gecko::u8
{
  Truncate,  // open and truncate to zero before writing
  Append,    // open and seek to end before writing
};

// Move-only owner of a buffer read from disk. Test for success via Ok()
// or operator bool. On failure, Data() is empty.
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
  [[nodiscard]] ::std::span<const ::std::byte> Data() const noexcept;
  [[nodiscard]] ::std::size_t Size() const noexcept
  {
    return m_Bytes.size();
  }

  // Take ownership of the underlying buffer. After this the result
  // reverts to a default-constructed (empty, !Ok) state.
  [[nodiscard]] ::std::vector<::std::byte> Take() noexcept;

private:
  ::std::vector<::std::byte> m_Bytes {};
  bool m_Ok {false};
};

// Move-only RAII holder of a memory mapping. The deleter is supplied by
// the backend and frees the platform mapping handle when the object is
// destroyed or moved-into.
class GECKO_API MappedFile
{
public:
  using Deleter = void (*)(void* handle) noexcept;

  MappedFile() noexcept = default;
  MappedFile(const ::std::byte* data, ::std::size_t size, void* handle,
             Deleter deleter) noexcept;

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
  [[nodiscard]] ::std::span<const ::std::byte> Data() const noexcept;
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

struct WriteResult
{
  bool Ok {false};
  ::gecko::u64 BytesWritten {0};

  explicit operator bool() const noexcept
  {
    return Ok;
  }
};

// Streaming write handle. Returned from IPlatformIO::OpenWrite and
// owned by the caller via ::gecko::Unique. Destroying the handle
// closes the underlying file. All methods are noexcept; on error
// they return false / 0 and leave the handle usable for further
// attempts.
struct IFileWriter
{
  GECKO_API virtual ~IFileWriter() = default;

  // Writes the full span. Returns true on success, false on partial
  // write or error.
  GECKO_API virtual bool Write(
      ::std::span<const ::std::byte> data) noexcept = 0;

  // Convenience overload for text payloads.
  GECKO_API bool WriteString(::std::string_view text) noexcept;

  // Flushes any buffered bytes to the OS. Does not fsync.
  GECKO_API virtual bool Flush() noexcept = 0;

  // Absolute seek from the start of the file. Negative offset means
  // "from end" (offset of -2 = two bytes before EOF). Returns the
  // resulting absolute position, or u64(-1) on failure.
  GECKO_API virtual ::gecko::u64 Seek(::gecko::i64 offset,
                                      bool fromEnd) noexcept = 0;

  // Current absolute position, or u64(-1) on failure.
  [[nodiscard]] GECKO_API virtual ::gecko::u64 Tell() noexcept = 0;
};

// One result of a directory iteration. Name is the basename only (no
// path separator); the caller may join with the directory path if it
// needs the full path.
struct DirEntry
{
  ::std::string Name {};
  bool IsDirectory {false};
};

// Move-only iterator over directory contents. Use:
//   auto it = io.IterateDir("logs");
//   DirEntry entry;
//   while (it.Next(entry)) { ... }
class GECKO_API DirIter
{
public:
  using NextFn = bool (*)(void* handle, DirEntry& out) noexcept;
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

  // Reads the next entry into out. Returns true if an entry was
  // produced, false at end-of-directory or on error.
  [[nodiscard]] bool Next(DirEntry& out) noexcept;

  // Releases the platform handle eagerly. Safe to call repeatedly.
  void Close() noexcept;

private:
  void* m_Handle {nullptr};
  NextFn m_Next {nullptr};
  CloseFn m_Close {nullptr};
};

// Platform-provided filesystem service. Published by PlatformModule;
// access via ::gecko::platform::GetPlatformIO() which never returns
// null (falls back to NullPlatformIO when no PlatformModule is active).
//
// Path arguments are forward-slash PathView values. Backends are
// responsible for normalising to native syscall conventions internally.
struct IPlatformIO
{
  GECKO_API virtual ~IPlatformIO() = default;

  // Read --------------------------------------------------------------
  [[nodiscard]] GECKO_API virtual bool Exists(PathView path) noexcept = 0;
  [[nodiscard]] GECKO_API virtual ::std::optional<FileStat> Stat(
      PathView path) noexcept = 0;
  [[nodiscard]] GECKO_API virtual ReadResult Read(PathView path) noexcept = 0;
  [[nodiscard]] GECKO_API virtual MappedFile Map(PathView path) noexcept = 0;

  // Write -------------------------------------------------------------
  GECKO_API virtual WriteResult Write(PathView path,
                                      ::std::span<const ::std::byte> data,
                                      WriteMode mode) noexcept = 0;

  // Atomic write: writes data to a sibling .tmp file and renames over
  // the target on success. Returns false if any step fails; the .tmp
  // file is removed on failure when possible.
  [[nodiscard]] GECKO_API virtual bool AtomicWrite(
      PathView path, ::std::span<const ::std::byte> data) noexcept = 0;

  // Opens path for streaming writes. Parent directory must already
  // exist (callers can ensure via CreateDir(..., true)). Returns null
  // on failure. Append mode positions the cursor at end-of-file but
  // does NOT enable POSIX O_APPEND atomic-append semantics; subsequent
  // Seek + Write calls may overwrite anywhere in the file.
  [[nodiscard]] GECKO_API virtual ::gecko::Unique<IFileWriter> OpenWrite(
      PathView path, WriteMode mode) noexcept = 0;

  // Filesystem --------------------------------------------------------
  GECKO_API virtual bool CreateDir(PathView path, bool recursive) noexcept = 0;
  GECKO_API virtual bool Remove(PathView path) noexcept = 0;
  [[nodiscard]] GECKO_API virtual DirIter IterateDir(
      PathView path) noexcept = 0;

  // Well-known paths --------------------------------------------------
  // Returned as owning std::string in forward-slash form. Empty string
  // on failure. ExePath includes the executable name; WorkingDir does
  // not.
  [[nodiscard]] GECKO_API virtual ::std::string ExePath() noexcept = 0;
  [[nodiscard]] GECKO_API virtual ::std::string WorkingDir() noexcept = 0;
  [[nodiscard]] GECKO_API virtual ::std::string UserDataDir(
      ::std::string_view appName) noexcept = 0;
};

// Default fallback used when no PlatformModule has booted. Reports no
// files exist; all reads fail; all writes fail. Suitable for unit tests
// that do not exercise IO and want a deterministic deny-all environment.
struct GECKO_API NullPlatformIO final : IPlatformIO
{
  [[nodiscard]] bool Exists(PathView /*path*/) noexcept override
  {
    return false;
  }
  [[nodiscard]] ::std::optional<FileStat> Stat(
      PathView /*path*/) noexcept override
  {
    return ::std::nullopt;
  }
  [[nodiscard]] ReadResult Read(PathView /*path*/) noexcept override
  {
    return {};
  }
  [[nodiscard]] MappedFile Map(PathView /*path*/) noexcept override
  {
    return {};
  }
  WriteResult Write(PathView /*path*/, ::std::span<const ::std::byte> /*data*/,
                    WriteMode /*mode*/) noexcept override
  {
    return {};
  }
  [[nodiscard]] bool AtomicWrite(
      PathView /*path*/,
      ::std::span<const ::std::byte> /*data*/) noexcept override
  {
    return false;
  }
  [[nodiscard]] ::gecko::Unique<IFileWriter> OpenWrite(
      PathView /*path*/, WriteMode /*mode*/) noexcept override
  {
    return {};
  }
  bool CreateDir(PathView /*path*/, bool /*recursive*/) noexcept override
  {
    return false;
  }
  bool Remove(PathView /*path*/) noexcept override
  {
    return false;
  }
  [[nodiscard]] DirIter IterateDir(PathView /*path*/) noexcept override
  {
    return {};
  }
  [[nodiscard]] ::std::string ExePath() noexcept override
  {
    return {};
  }
  [[nodiscard]] ::std::string WorkingDir() noexcept override
  {
    return {};
  }
  [[nodiscard]] ::std::string UserDataDir(
      ::std::string_view /*appName*/) noexcept override
  {
    return {};
  }
};

[[nodiscard]] GECKO_API IPlatformIO* GetPlatformIO() noexcept;

}  // namespace gecko::platform
