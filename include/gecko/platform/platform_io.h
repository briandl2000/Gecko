#pragma once

#include "gecko/core/api.h"
#include "gecko/core/array.h"
#include "gecko/core/optional.h"
#include "gecko/core/ptr.h"
#include "gecko/core/span.h"
#include "gecko/core/string.h"
#include "gecko/core/types.h"
#include "gecko/platform/path_view.h"

namespace gecko::platform {

struct FileStat
{
  u64 Size {0};
  i64 MTimeEpoch {-1};
  bool IsDirectory {false};
};

enum class WriteMode : u8
{
  Truncate,
  Append,
};

struct WriteResult
{
  bool Ok {false};
  u64 BytesWritten {0};

  explicit operator bool() const noexcept
  {
    return Ok;
  }
};

struct DirEntry
{
  String Name;
  bool IsDirectory {false};
};

class GECKO_API ReadResult
{
public:
  ReadResult() noexcept = default;
  explicit ReadResult(Array<byte> bytes) noexcept;
  ReadResult(const ReadResult&) = delete;
  ReadResult& operator=(const ReadResult&) = delete;
  ReadResult(ReadResult&& other) noexcept;
  ReadResult& operator=(ReadResult&& other) noexcept;

  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Ok;
  }
  explicit operator bool() const noexcept
  {
    return m_Ok;
  }
  [[nodiscard]] Span<const byte> Data() const noexcept
  {
    return {m_Bytes.Data(), m_Bytes.Count()};
  }
  [[nodiscard]] usize Size() const noexcept
  {
    return m_Bytes.Count();
  }
  [[nodiscard]] Array<byte> Take() noexcept;

private:
  Array<byte> m_Bytes;
  bool m_Ok {false};
};

class GECKO_API MappedFile
{
public:
  using Deleter = void (*)(void* handle) noexcept;

  MappedFile() noexcept = default;
  MappedFile(const byte* data, usize size, void* handle, Deleter deleter) noexcept;
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
  [[nodiscard]] Span<const byte> Data() const noexcept
  {
    return {m_Data, m_Size};
  }
  [[nodiscard]] usize Size() const noexcept
  {
    return m_Size;
  }

private:
  void Reset() noexcept;
  const byte* m_Data {nullptr};
  usize m_Size {0};
  void* m_Handle {nullptr};
  Deleter m_Deleter {nullptr};
};

class GECKO_API DirIter
{
public:
  using NextFn = bool (*)(void* handle, DirEntry* output) noexcept;
  using CloseFn = void (*)(void* handle) noexcept;

  DirIter() noexcept = default;
  DirIter(void* handle, NextFn next, CloseFn close) noexcept;
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
  [[nodiscard]] Optional<DirEntry> Next() noexcept;
  void Close() noexcept;

private:
  void* m_Handle {nullptr};
  NextFn m_Next {nullptr};
  CloseFn m_Close {nullptr};
};

class GECKO_API FileWriter
{
public:
  virtual ~FileWriter() = default;
  virtual bool Write(Span<const byte> data) noexcept = 0;
  bool WriteString(StringView text) noexcept;
  virtual bool Flush() noexcept = 0;
  virtual u64 Seek(i64 offset, bool fromEnd) noexcept = 0;
  [[nodiscard]] virtual u64 Tell() noexcept = 0;
};

[[nodiscard]] GECKO_API bool Exists(PathView path) noexcept;
[[nodiscard]] GECKO_API Optional<FileStat> Stat(PathView path) noexcept;
[[nodiscard]] GECKO_API ReadResult Read(PathView path) noexcept;
[[nodiscard]] GECKO_API MappedFile Map(PathView path) noexcept;
GECKO_API WriteResult Write(PathView path, Span<const byte> data, WriteMode mode) noexcept;
[[nodiscard]] GECKO_API bool AtomicWrite(PathView path, Span<const byte> data) noexcept;
[[nodiscard]] GECKO_API Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept;
GECKO_API bool CreateDir(PathView path, bool recursive) noexcept;
GECKO_API bool Remove(PathView path) noexcept;
[[nodiscard]] GECKO_API DirIter IterateDir(PathView path) noexcept;
[[nodiscard]] GECKO_API String ExePath() noexcept;
[[nodiscard]] GECKO_API String WorkingDir() noexcept;
[[nodiscard]] GECKO_API String UserDataDir(StringView appName) noexcept;

}  // namespace gecko::platform
