#if defined(GECKO_PLATFORM_WINDOWS)

#include "platform_io_win32.h"

#include "gecko/core/ptr.h"
#include "gecko/platform/path_view.h"
#include "gecko/platform/platform_io.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off: <Windows.h> must be included before any sub-header
// (e.g. <ShlObj.h>, <fileapi.h>) so that the architecture macros it sets
// up are visible to winnt.h.
#include <Windows.h>

#include <ShlObj.h>
#include <fileapi.h>

#include <algorithm>
#include <string>
#include <vector>
// clang-format on

namespace gecko::platform {

namespace win32_io {

::std::wstring ToWide(PathView path) noexcept
{
  if (path.Empty())
    return {};
  auto sv = path.View();
  int wLen = ::MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
  if (wLen <= 0)
    return {};
  ::std::wstring out;
  try
  {
    out.resize(static_cast<::std::size_t>(wLen));
  }
  catch (...)
  {
    return {};
  }
  ::MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), out.data(), wLen);
  for (auto& c : out)
  {
    if (c == L'/')
      c = L'\\';
  }
  return out;
}

::std::string FromWide(const wchar_t* wide, ::std::size_t wLen) noexcept
{
  if (wide == nullptr || wLen == 0)
    return {};
  int u8Len = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wLen), nullptr, 0, nullptr, nullptr);
  if (u8Len <= 0)
    return {};
  ::std::string out;
  try
  {
    out.resize(static_cast<::std::size_t>(u8Len));
  }
  catch (...)
  {
    return {};
  }
  ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wLen), out.data(), u8Len, nullptr, nullptr);
  for (auto& c : out)
  {
    if (c == '\\')
      c = '/';
  }
  return out;
}

}  // namespace win32_io

namespace {

using win32_io::FromWide;
using win32_io::ToWide;

class Win32FileWriter final : public FileWriter
{
public:
  explicit Win32FileWriter(HANDLE h) noexcept : m_Handle(h)
  {}

  ~Win32FileWriter() noexcept override
  {
    if (m_Handle != INVALID_HANDLE_VALUE)
      ::CloseHandle(m_Handle);
  }

  bool Write(::std::span<const ::std::byte> data) noexcept override
  {
    if (m_Handle == INVALID_HANDLE_VALUE)
      return false;
    ::std::size_t total = 0;
    while (total < data.size())
    {
      DWORD chunk = static_cast<DWORD>(::std::min<::std::size_t>(data.size() - total, 1u << 24));
      DWORD wrote = 0;
      if (!::WriteFile(m_Handle, data.data() + total, chunk, &wrote, nullptr))
        return false;
      total += wrote;
    }
    return true;
  }

  bool Flush() noexcept override
  {
    if (m_Handle == INVALID_HANDLE_VALUE)
      return false;
    return ::FlushFileBuffers(m_Handle) != 0;
  }

  ::gecko::u64 Seek(::gecko::i64 offset, bool fromEnd) noexcept override
  {
    if (m_Handle == INVALID_HANDLE_VALUE)
      return static_cast<::gecko::u64>(-1);
    LARGE_INTEGER li {};
    li.QuadPart = offset;
    LARGE_INTEGER out {};
    if (!::SetFilePointerEx(m_Handle, li, &out, fromEnd ? FILE_END : FILE_BEGIN))
      return static_cast<::gecko::u64>(-1);
    return static_cast<::gecko::u64>(out.QuadPart);
  }

  ::gecko::u64 Tell() noexcept override
  {
    if (m_Handle == INVALID_HANDLE_VALUE)
      return static_cast<::gecko::u64>(-1);
    LARGE_INTEGER zero {};
    LARGE_INTEGER out {};
    if (!::SetFilePointerEx(m_Handle, zero, &out, FILE_CURRENT))
      return static_cast<::gecko::u64>(-1);
    return static_cast<::gecko::u64>(out.QuadPart);
  }

private:
  HANDLE m_Handle {INVALID_HANDLE_VALUE};
};

}  // namespace

// -- Public free-function impls --------------------------------------

bool Exists(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return false;
  DWORD attrs = ::GetFileAttributesW(w.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES;
}

::std::optional<FileStat> Stat(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return ::std::nullopt;
  WIN32_FILE_ATTRIBUTE_DATA data {};
  if (!::GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &data))
    return ::std::nullopt;
  FileStat fs {};
  ULARGE_INTEGER size {};
  size.LowPart = data.nFileSizeLow;
  size.HighPart = data.nFileSizeHigh;
  fs.Size = size.QuadPart;
  ULARGE_INTEGER ft {};
  ft.LowPart = data.ftLastWriteTime.dwLowDateTime;
  ft.HighPart = data.ftLastWriteTime.dwHighDateTime;
  constexpr ::gecko::u64 FiletimeUnixDelta = 116444736000000000ULL;
  if (ft.QuadPart >= FiletimeUnixDelta)
    fs.MTimeEpoch = static_cast<::gecko::i64>((ft.QuadPart - FiletimeUnixDelta) / 10000000ULL);
  fs.IsDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return fs;
}

ReadResult Read(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return {};
  HANDLE h =
      ::CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return {};
  LARGE_INTEGER size {};
  if (!::GetFileSizeEx(h, &size))
  {
    ::CloseHandle(h);
    return {};
  }
  ::std::vector<::std::byte> buf;
  try
  {
    buf.resize(static_cast<::std::size_t>(size.QuadPart));
  }
  catch (...)
  {
    ::CloseHandle(h);
    return {};
  }
  ::std::size_t total = 0;
  while (total < buf.size())
  {
    DWORD chunk = static_cast<DWORD>(::std::min<::std::size_t>(buf.size() - total, 1u << 24));
    DWORD got = 0;
    if (!::ReadFile(h, buf.data() + total, chunk, &got, nullptr))
    {
      ::CloseHandle(h);
      return {};
    }
    if (got == 0)
    {
      buf.resize(total);
      break;
    }
    total += got;
  }
  ::CloseHandle(h);
  return ReadResult {::std::move(buf)};
}

MappedFile Map(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return {};
  HANDLE file =
      ::CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return {};
  LARGE_INTEGER size {};
  if (!::GetFileSizeEx(file, &size) || size.QuadPart == 0)
  {
    ::CloseHandle(file);
    return {};
  }
  HANDLE map = ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (map == nullptr)
  {
    ::CloseHandle(file);
    return {};
  }
  void* view = ::MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
  if (view == nullptr)
  {
    ::CloseHandle(map);
    ::CloseHandle(file);
    return {};
  }

  struct Mapping
  {
    void* View;
    HANDLE Map;
    HANDLE File;
  };
  auto* m = new (::std::nothrow) Mapping {view, map, file};
  if (m == nullptr)
  {
    ::UnmapViewOfFile(view);
    ::CloseHandle(map);
    ::CloseHandle(file);
    return {};
  }

  auto deleter = [](void* handle) noexcept {
    auto* mapping = static_cast<Mapping*>(handle);
    ::UnmapViewOfFile(mapping->View);
    ::CloseHandle(mapping->Map);
    ::CloseHandle(mapping->File);
    delete mapping;
  };

  return MappedFile {static_cast<const ::std::byte*>(view), static_cast<::std::size_t>(size.QuadPart), m, deleter};
}

WriteResult Write(PathView path, ::std::span<const ::std::byte> data, WriteMode mode) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return {};
  DWORD disp = (mode == WriteMode::Truncate) ? CREATE_ALWAYS : OPEN_ALWAYS;
  HANDLE h = ::CreateFileW(w.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, disp, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return {};
  if (mode == WriteMode::Append)
    ::SetFilePointer(h, 0, nullptr, FILE_END);

  ::std::size_t total = 0;
  while (total < data.size())
  {
    DWORD chunk = static_cast<DWORD>(::std::min<::std::size_t>(data.size() - total, 1u << 24));
    DWORD wrote = 0;
    if (!::WriteFile(h, data.data() + total, chunk, &wrote, nullptr))
    {
      ::CloseHandle(h);
      return {};
    }
    total += wrote;
  }
  ::CloseHandle(h);
  return WriteResult {.Ok = true, .BytesWritten = static_cast<::gecko::u64>(total)};
}

bool AtomicWrite(PathView path, ::std::span<const ::std::byte> data) noexcept
{
  auto target = ToWide(path);
  if (target.empty())
    return false;
  auto tmp = target + L".tmp";

  HANDLE h = ::CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;

  ::std::size_t total = 0;
  while (total < data.size())
  {
    DWORD chunk = static_cast<DWORD>(::std::min<::std::size_t>(data.size() - total, 1u << 24));
    DWORD wrote = 0;
    if (!::WriteFile(h, data.data() + total, chunk, &wrote, nullptr))
    {
      ::CloseHandle(h);
      ::DeleteFileW(tmp.c_str());
      return false;
    }
    total += wrote;
  }
  ::FlushFileBuffers(h);
  ::CloseHandle(h);

  if (!::MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
  {
    ::DeleteFileW(tmp.c_str());
    return false;
  }
  return true;
}

::gecko::Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return {};
  DWORD disposition = (mode == WriteMode::Append) ? OPEN_ALWAYS : CREATE_ALWAYS;
  HANDLE h =
      ::CreateFileW(w.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return {};
  if (mode == WriteMode::Append)
  {
    LARGE_INTEGER zero {};
    LARGE_INTEGER out {};
    ::SetFilePointerEx(h, zero, &out, FILE_END);
  }
  return ::gecko::CreateUnique<Win32FileWriter>(h);
}

bool CreateDir(PathView path, bool recursive) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return false;

  if (!recursive)
  {
    if (::CreateDirectoryW(w.c_str(), nullptr))
      return true;
    return ::GetLastError() == ERROR_ALREADY_EXISTS;
  }

  for (::std::size_t i = 1; i <= w.size(); ++i)
  {
    if (i == w.size() || w[i] == L'\\')
    {
      // Skip the drive-letter prefix (e.g. "C:") -- CreateDirectoryW would
      // fail.
      if (i == 2 && w.size() >= 2 && w[1] == L':')
        continue;
      ::std::wstring sub(w, 0, i);
      if (sub.empty())
        continue;
      if (!::CreateDirectoryW(sub.c_str(), nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    }
  }
  return true;
}

bool Remove(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return false;
  DWORD attrs = ::GetFileAttributesW(w.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return false;
  if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    return ::RemoveDirectoryW(w.c_str()) != 0;
  return ::DeleteFileW(w.c_str()) != 0;
}

DirIter IterateDir(PathView path) noexcept
{
  auto w = ToWide(path);
  if (w.empty())
    return {};
  if (w.back() != L'\\' && w.back() != L'/')
    w.push_back(L'\\');
  w.push_back(L'*');

  struct State
  {
    HANDLE Handle;
    WIN32_FIND_DATAW Data;
    bool HasFirst;
  };

  WIN32_FIND_DATAW data {};
  HANDLE h = ::FindFirstFileW(w.c_str(), &data);
  if (h == INVALID_HANDLE_VALUE)
    return {};

  auto* state = new (::std::nothrow) State {h, data, true};
  if (state == nullptr)
  {
    ::FindClose(h);
    return {};
  }

  auto next = [](void* handle, DirEntry* out) noexcept -> bool {
    auto* st = static_cast<State*>(handle);
    for (;;)
    {
      if (!st->HasFirst)
      {
        if (!::FindNextFileW(st->Handle, &st->Data))
          return false;
      }
      st->HasFirst = false;
      if (st->Data.cFileName[0] == L'.' &&
          (st->Data.cFileName[1] == L'\0' || (st->Data.cFileName[1] == L'.' && st->Data.cFileName[2] == L'\0')))
        continue;

      ::std::size_t len = 0;
      while (len < MAX_PATH && st->Data.cFileName[len] != L'\0')
        ++len;
      try
      {
        out->Name = FromWide(st->Data.cFileName, len);
      }
      catch (...)
      {
        return false;
      }
      out->IsDirectory = (st->Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      return true;
    }
  };
  auto close = [](void* handle) noexcept {
    auto* st = static_cast<State*>(handle);
    ::FindClose(st->Handle);
    delete st;
  };
  return DirIter {state, next, close};
}

::std::string ExePath() noexcept
{
  wchar_t buf[MAX_PATH];
  DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n == MAX_PATH)
    return {};
  return FromWide(buf, n);
}

::std::string WorkingDir() noexcept
{
  DWORD n = ::GetCurrentDirectoryW(0, nullptr);
  if (n == 0)
    return {};
  ::std::wstring buf;
  try
  {
    buf.resize(n);
  }
  catch (...)
  {
    return {};
  }
  DWORD got = ::GetCurrentDirectoryW(n, buf.data());
  if (got == 0 || got >= n)
    return {};
  return FromWide(buf.data(), got);
}

::std::string UserDataDir(::std::string_view appName) noexcept
{
  PWSTR rawPath = nullptr;
  HRESULT hr = ::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &rawPath);
  if (FAILED(hr) || rawPath == nullptr)
  {
    if (rawPath != nullptr)
      ::CoTaskMemFree(rawPath);
    return {};
  }
  ::std::size_t len = 0;
  while (rawPath[len] != L'\0')
    ++len;
  auto base = FromWide(rawPath, len);
  ::CoTaskMemFree(rawPath);
  if (base.empty())
    return {};
  if (!appName.empty())
  {
    try
    {
      base.push_back('/');
      base.append(appName);
    }
    catch (...)
    {
      return {};
    }
  }
  return base;
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_WINDOWS
