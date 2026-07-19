#if defined(GECKO_PLATFORM_WINDOWS)

#include "platform_io_win32.h"

#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off: Windows.h must establish the target before ShlObj.h.
#include <Windows.h>
#include <ShlObj.h>
// clang-format on

namespace gecko::platform {

namespace {

struct WidePath
{
  Array<wchar_t> Characters;

  [[nodiscard]] bool Empty() const noexcept
  {
    return Characters.Count() <= 1;
  }
  [[nodiscard]] wchar_t* Data() noexcept
  {
    return Characters.Data();
  }
  [[nodiscard]] const wchar_t* Data() const noexcept
  {
    return Characters.Data();
  }
  [[nodiscard]] usize Count() const noexcept
  {
    return Empty() ? 0 : Characters.Count() - 1U;
  }
  void Append(wchar_t character) noexcept
  {
    if (!Characters.Empty())
      Characters.PopBack();
    Characters.PushBack(character);
    Characters.PushBack(L'\0');
  }
  void Append(const wchar_t* text) noexcept
  {
    if (!Characters.Empty())
      Characters.PopBack();
    while (*text != L'\0')
      Characters.PushBack(*text++);
    Characters.PushBack(L'\0');
  }
};

WidePath ToWide(PathView path) noexcept
{
  WidePath output;
  if (path.Empty())
    return output;
  const int count = ::MultiByteToWideChar(CP_UTF8, 0, path.Data(), static_cast<int>(path.Size()), nullptr, 0);
  if (count <= 0)
    return output;
  output.Characters.Resize(static_cast<usize>(count) + 1U);
  (void)::MultiByteToWideChar(CP_UTF8, 0, path.Data(), static_cast<int>(path.Size()), output.Data(), count);
  output.Characters[count] = L'\0';
  for (int index = 0; index < count; ++index)
    if (output.Characters[index] == L'/')
      output.Characters[index] = L'\\';
  return output;
}

String FromWide(const wchar_t* wide, usize count) noexcept
{
  if (wide == nullptr || count == 0)
    return {};
  const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(count), nullptr, 0, nullptr, nullptr);
  if (size <= 0)
    return {};
  String output;
  output.Resize(static_cast<usize>(size));
  (void)::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(count), output.Data(), size, nullptr, nullptr);
  for (usize index = 0; index < output.Count(); ++index)
    if (output[index] == '\\')
      output[index] = '/';
  return output;
}

bool WriteHandle(HANDLE handle, Span<const byte> data) noexcept
{
  usize total = 0;
  while (total < data.Count())
  {
    const usize remaining = data.Count() - total;
    const DWORD chunk = static_cast<DWORD>(remaining < (1U << 24U) ? remaining : (1U << 24U));
    DWORD written = 0;
    if (!::WriteFile(handle, data.Data() + total, chunk, &written, nullptr) || written == 0)
      return false;
    total += written;
  }
  return true;
}

class Win32FileWriter final : public FileWriter
{
public:
  explicit Win32FileWriter(HANDLE handle) noexcept : m_Handle(handle)
  {}
  ~Win32FileWriter() noexcept override
  {
    if (m_Handle != INVALID_HANDLE_VALUE)
      (void)::CloseHandle(m_Handle);
  }
  bool Write(Span<const byte> data) noexcept override
  {
    return m_Handle != INVALID_HANDLE_VALUE && WriteHandle(m_Handle, data);
  }
  bool Flush() noexcept override
  {
    return m_Handle != INVALID_HANDLE_VALUE && ::FlushFileBuffers(m_Handle) != 0;
  }
  u64 Seek(i64 offset, bool fromEnd) noexcept override
  {
    LARGE_INTEGER input {};
    LARGE_INTEGER output {};
    input.QuadPart = offset;
    return m_Handle != INVALID_HANDLE_VALUE &&
                   ::SetFilePointerEx(m_Handle, input, &output, fromEnd ? FILE_END : FILE_BEGIN)
               ? static_cast<u64>(output.QuadPart)
               : U64Max;
  }
  u64 Tell() noexcept override
  {
    LARGE_INTEGER zero {};
    LARGE_INTEGER output {};
    return m_Handle != INVALID_HANDLE_VALUE && ::SetFilePointerEx(m_Handle, zero, &output, FILE_CURRENT)
               ? static_cast<u64>(output.QuadPart)
               : U64Max;
  }

private:
  HANDLE m_Handle {INVALID_HANDLE_VALUE};
};

struct Mapping
{
  void* View {nullptr};
  HANDLE Map {nullptr};
  HANDLE File {INVALID_HANDLE_VALUE};
};

void CloseMapping(void* opaque) noexcept
{
  auto* mapping = static_cast<Mapping*>(opaque);
  (void)::UnmapViewOfFile(mapping->View);
  (void)::CloseHandle(mapping->Map);
  (void)::CloseHandle(mapping->File);
  mapping->~Mapping();
  DeallocBytes(mapping);
}

}  // namespace

bool Exists(PathView path) noexcept
{
  const WidePath native = ToWide(path);
  return !native.Empty() && ::GetFileAttributesW(native.Data()) != INVALID_FILE_ATTRIBUTES;
}

Optional<FileStat> Stat(PathView path) noexcept
{
  const WidePath native = ToWide(path);
  WIN32_FILE_ATTRIBUTE_DATA data {};
  if (native.Empty() || !::GetFileAttributesExW(native.Data(), GetFileExInfoStandard, &data))
    return {};
  ULARGE_INTEGER size {};
  size.LowPart = data.nFileSizeLow;
  size.HighPart = data.nFileSizeHigh;
  ULARGE_INTEGER time {};
  time.LowPart = data.ftLastWriteTime.dwLowDateTime;
  time.HighPart = data.ftLastWriteTime.dwHighDateTime;
  constexpr u64 FileTimeUnixDelta = 116444736000000000ULL;
  return FileStat {
      .Size = size.QuadPart,
      .MTimeEpoch =
          time.QuadPart >= FileTimeUnixDelta ? static_cast<i64>((time.QuadPart - FileTimeUnixDelta) / 10000000ULL) : -1,
      .IsDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
  };
}

ReadResult Read(PathView path) noexcept
{
  const WidePath native = ToWide(path);
  HANDLE file = native.Empty() ? INVALID_HANDLE_VALUE
                               : ::CreateFileW(native.Data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return {};
  LARGE_INTEGER fileSize {};
  if (!::GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 0)
  {
    (void)::CloseHandle(file);
    return {};
  }
  Array<byte> bytes(static_cast<usize>(fileSize.QuadPart));
  usize total = 0;
  while (total < bytes.Count())
  {
    const usize remaining = bytes.Count() - total;
    const DWORD chunk = static_cast<DWORD>(remaining < (1U << 24U) ? remaining : (1U << 24U));
    DWORD read = 0;
    if (!::ReadFile(file, bytes.Data() + total, chunk, &read, nullptr))
    {
      (void)::CloseHandle(file);
      return {};
    }
    if (read == 0)
      break;
    total += read;
  }
  (void)::CloseHandle(file);
  bytes.Resize(total);
  return ReadResult {Move(bytes)};
}

MappedFile Map(PathView path) noexcept
{
  const WidePath native = ToWide(path);
  HANDLE file = native.Empty() ? INVALID_HANDLE_VALUE
                               : ::CreateFileW(native.Data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, nullptr);
  LARGE_INTEGER size {};
  if (file == INVALID_HANDLE_VALUE || !::GetFileSizeEx(file, &size) || size.QuadPart <= 0)
  {
    if (file != INVALID_HANDLE_VALUE)
      (void)::CloseHandle(file);
    return {};
  }
  HANDLE map = ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  void* view = map != nullptr ? ::MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0) : nullptr;
  if (view == nullptr)
  {
    if (map != nullptr)
      (void)::CloseHandle(map);
    (void)::CloseHandle(file);
    return {};
  }
  void* storage = AllocBytes(sizeof(Mapping), alignof(Mapping));
  auto* mapping = new (storage, Placement) Mapping {.View = view, .Map = map, .File = file};
  return MappedFile {static_cast<const byte*>(view), static_cast<usize>(size.QuadPart), mapping, CloseMapping};
}

WriteResult Write(PathView path, Span<const byte> data, WriteMode mode) noexcept
{
  const WidePath native = ToWide(path);
  const DWORD disposition = mode == WriteMode::Truncate ? CREATE_ALWAYS : OPEN_ALWAYS;
  HANDLE file = native.Empty() ? INVALID_HANDLE_VALUE
                               : ::CreateFileW(native.Data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, disposition,
                                               FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return {};
  if (mode == WriteMode::Append)
    (void)::SetFilePointer(file, 0, nullptr, FILE_END);
  const bool ok = WriteHandle(file, data);
  (void)::CloseHandle(file);
  return WriteResult {.Ok = ok, .BytesWritten = ok ? static_cast<u64>(data.Count()) : 0};
}

bool AtomicWrite(PathView path, Span<const byte> data) noexcept
{
  const WidePath target = ToWide(path);
  if (target.Empty())
    return false;
  WidePath temporary = target;
  temporary.Append(L".tmp");
  HANDLE file =
      ::CreateFileW(temporary.Data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  const bool ok = WriteHandle(file, data) && ::FlushFileBuffers(file);
  (void)::CloseHandle(file);
  if (!ok || !::MoveFileExW(temporary.Data(), target.Data(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
  {
    (void)::DeleteFileW(temporary.Data());
    return false;
  }
  return true;
}

Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept
{
  const WidePath native = ToWide(path);
  const DWORD disposition = mode == WriteMode::Append ? OPEN_ALWAYS : CREATE_ALWAYS;
  HANDLE file = native.Empty() ? INVALID_HANDLE_VALUE
                               : ::CreateFileW(native.Data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, disposition,
                                               FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return {};
  if (mode == WriteMode::Append)
  {
    LARGE_INTEGER zero {};
    (void)::SetFilePointerEx(file, zero, nullptr, FILE_END);
  }
  return CreateUnique<Win32FileWriter>(file);
}

bool CreateDir(PathView path, bool recursive) noexcept
{
  const WidePath native = ToWide(path);
  if (native.Empty())
    return false;
  if (!recursive)
    return ::CreateDirectoryW(native.Data(), nullptr) || ::GetLastError() == ERROR_ALREADY_EXISTS;
  for (usize index = 1; index <= native.Count(); ++index)
  {
    if (index != native.Count() && native.Data()[index] != L'\\')
      continue;
    if (index == 2 && native.Data()[1] == L':')
      continue;
    WidePath part;
    part.Characters.Resize(index + 1U);
    for (usize cursor = 0; cursor < index; ++cursor)
      part.Characters[cursor] = native.Data()[cursor];
    part.Characters[index] = L'\0';
    if (!::CreateDirectoryW(part.Data(), nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }
  return true;
}

bool Remove(PathView path) noexcept
{
  const WidePath native = ToWide(path);
  const DWORD attributes = native.Empty() ? INVALID_FILE_ATTRIBUTES : ::GetFileAttributesW(native.Data());
  if (attributes == INVALID_FILE_ATTRIBUTES)
    return false;
  return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? ::RemoveDirectoryW(native.Data()) != 0
                                                      : ::DeleteFileW(native.Data()) != 0;
}

DirIter IterateDir(PathView path) noexcept
{
  WidePath native = ToWide(path);
  if (native.Empty())
    return {};
  if (native.Data()[native.Count() - 1U] != L'\\')
    native.Append(L'\\');
  native.Append(L'*');

  struct State
  {
    HANDLE Handle {INVALID_HANDLE_VALUE};
    WIN32_FIND_DATAW Data {};
    bool HasFirst {true};
  };
  WIN32_FIND_DATAW data {};
  HANDLE search = ::FindFirstFileW(native.Data(), &data);
  if (search == INVALID_HANDLE_VALUE)
    return {};
  void* storage = AllocBytes(sizeof(State), alignof(State));
  auto* state = new (storage, Placement) State {.Handle = search, .Data = data};
  auto next = [](void* opaque, DirEntry* output) noexcept -> bool {
    auto* value = static_cast<State*>(opaque);
    for (;;)
    {
      if (!value->HasFirst && !::FindNextFileW(value->Handle, &value->Data))
        return false;
      value->HasFirst = false;
      if (value->Data.cFileName[0] == L'.' && (value->Data.cFileName[1] == L'\0' ||
                                               (value->Data.cFileName[1] == L'.' && value->Data.cFileName[2] == L'\0')))
        continue;
      usize length = 0;
      while (value->Data.cFileName[length] != L'\0')
        ++length;
      output->Name = FromWide(value->Data.cFileName, length);
      output->IsDirectory = (value->Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      return true;
    }
  };
  auto close = [](void* opaque) noexcept {
    auto* value = static_cast<State*>(opaque);
    (void)::FindClose(value->Handle);
    value->~State();
    DeallocBytes(value);
  };
  return DirIter {state, next, close};
}

String ExePath() noexcept
{
  wchar_t buffer[MAX_PATH];
  const DWORD count = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  return count != 0 && count != MAX_PATH ? FromWide(buffer, count) : String {};
}

String WorkingDir() noexcept
{
  const DWORD required = ::GetCurrentDirectoryW(0, nullptr);
  if (required == 0)
    return {};
  Array<wchar_t> buffer(required);
  const DWORD count = ::GetCurrentDirectoryW(required, buffer.Data());
  return count != 0 && count < required ? FromWide(buffer.Data(), count) : String {};
}

String UserDataDir(StringView appName) noexcept
{
  PWSTR raw = nullptr;
  if (FAILED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw)) || raw == nullptr)
    return {};
  usize count = 0;
  while (raw[count] != L'\0')
    ++count;
  String path = FromWide(raw, count);
  ::CoTaskMemFree(raw);
  if (!appName.Empty())
  {
    path.Append('/');
    path.Append(appName);
  }
  return path;
}

}  // namespace gecko::platform

#endif
