#include "gecko/platform/platform_io.h"

#include "gecko/core/services.h"
#include "gecko/core/services/modules.h"
#include "private/native_platform_io.h"

#include <utility>

namespace gecko::platform {

// ── PathView helpers ────────────────────────────────────────────────

bool PathView::IsAbsolute() const noexcept
{
  if (m_View.empty())
    return false;
  if (m_View.front() == '/')
    return true;
  // Drive-letter form: "C:/...". Engine paths are forward-slash so we do
  // not check for backslash. The Win32 backend re-normalises before
  // syscalls.
  if (m_View.size() >= 3 && m_View[1] == ':' && m_View[2] == '/')
  {
    char c = m_View.front();
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  }
  return false;
}

PathView PathView::ParentDir() const noexcept
{
  auto pos = m_View.rfind('/');
  if (pos == ::std::string_view::npos)
    return PathView {};
  if (pos == 0)
    return PathView {::std::string_view {"/"}};
  return PathView {m_View.substr(0, pos)};
}

PathView PathView::Filename() const noexcept
{
  auto pos = m_View.rfind('/');
  if (pos == ::std::string_view::npos)
    return *this;
  return PathView {m_View.substr(pos + 1)};
}

PathView PathView::Stem() const noexcept
{
  auto fname = Filename().View();
  if (fname.empty())
    return PathView {};
  // Skip a leading dot so ".bashrc" -> stem is "" (matches std::filesystem).
  ::std::size_t start = 0;
  while (start < fname.size() && fname[start] == '.')
    ++start;
  if (start == fname.size())
    return PathView {fname.substr(0, 0)};
  auto dot = fname.rfind('.');
  if (dot == ::std::string_view::npos || dot < start)
    return PathView {fname};
  return PathView {fname.substr(0, dot)};
}

PathView PathView::Extension() const noexcept
{
  auto fname = Filename().View();
  if (fname.empty())
    return PathView {};
  ::std::size_t start = 0;
  while (start < fname.size() && fname[start] == '.')
    ++start;
  if (start == fname.size())
    return PathView {};
  auto dot = fname.rfind('.');
  if (dot == ::std::string_view::npos || dot < start)
    return PathView {};
  return PathView {fname.substr(dot)};
}

// ── ReadResult ──────────────────────────────────────────────────────

ReadResult::ReadResult(::std::vector<::std::byte> bytes) noexcept
    : m_Bytes(::std::move(bytes)), m_Ok(true)
{}

ReadResult::ReadResult(ReadResult&& other) noexcept
    : m_Bytes(::std::move(other.m_Bytes)), m_Ok(other.m_Ok)
{
  other.m_Ok = false;
}

ReadResult& ReadResult::operator=(ReadResult&& other) noexcept
{
  if (this != &other)
  {
    m_Bytes = ::std::move(other.m_Bytes);
    m_Ok = other.m_Ok;
    other.m_Ok = false;
  }
  return *this;
}

ReadResult::~ReadResult() noexcept = default;

::std::span<const ::std::byte> ReadResult::Data() const noexcept
{
  return ::std::span<const ::std::byte> {m_Bytes.data(), m_Bytes.size()};
}

::std::vector<::std::byte> ReadResult::Take() noexcept
{
  m_Ok = false;
  return ::std::move(m_Bytes);
}

// ── MappedFile ──────────────────────────────────────────────────────

MappedFile::MappedFile(const ::std::byte* data, ::std::size_t size,
                       void* handle, Deleter deleter) noexcept
    : m_Data(data), m_Size(size), m_Handle(handle), m_Deleter(deleter)
{}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : m_Data(other.m_Data), m_Size(other.m_Size), m_Handle(other.m_Handle),
      m_Deleter(other.m_Deleter)
{
  other.m_Data = nullptr;
  other.m_Size = 0;
  other.m_Handle = nullptr;
  other.m_Deleter = nullptr;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
{
  if (this != &other)
  {
    Reset();
    m_Data = other.m_Data;
    m_Size = other.m_Size;
    m_Handle = other.m_Handle;
    m_Deleter = other.m_Deleter;
    other.m_Data = nullptr;
    other.m_Size = 0;
    other.m_Handle = nullptr;
    other.m_Deleter = nullptr;
  }
  return *this;
}

MappedFile::~MappedFile() noexcept
{
  Reset();
}

::std::span<const ::std::byte> MappedFile::Data() const noexcept
{
  return ::std::span<const ::std::byte> {m_Data, m_Size};
}

void MappedFile::Reset() noexcept
{
  if (m_Deleter != nullptr && m_Handle != nullptr)
    m_Deleter(m_Handle);
  m_Data = nullptr;
  m_Size = 0;
  m_Handle = nullptr;
  m_Deleter = nullptr;
}

// ── DirIter ─────────────────────────────────────────────────────────

DirIter::DirIter(void* handle, NextFn nextFn, CloseFn closeFn) noexcept
    : m_Handle(handle), m_Next(nextFn), m_Close(closeFn)
{}

DirIter::DirIter(DirIter&& other) noexcept
    : m_Handle(other.m_Handle), m_Next(other.m_Next), m_Close(other.m_Close)
{
  other.m_Handle = nullptr;
  other.m_Next = nullptr;
  other.m_Close = nullptr;
}

DirIter& DirIter::operator=(DirIter&& other) noexcept
{
  if (this != &other)
  {
    Close();
    m_Handle = other.m_Handle;
    m_Next = other.m_Next;
    m_Close = other.m_Close;
    other.m_Handle = nullptr;
    other.m_Next = nullptr;
    other.m_Close = nullptr;
  }
  return *this;
}

DirIter::~DirIter() noexcept
{
  Close();
}

bool DirIter::Next(DirEntry& out) noexcept
{
  if (m_Handle == nullptr || m_Next == nullptr)
    return false;
  return m_Next(m_Handle, out);
}

void DirIter::Close() noexcept
{
  if (m_Close != nullptr && m_Handle != nullptr)
    m_Close(m_Handle);
  m_Handle = nullptr;
  m_Next = nullptr;
  m_Close = nullptr;
}

// ── IFileWriter helper ──────────────────────────────────────────────

bool IFileWriter::WriteString(::std::string_view text) noexcept
{
  return Write(
      {reinterpret_cast<const ::std::byte*>(text.data()), text.size()});
}

// ── Accessor ────────────────────────────────────────────────────────

namespace {

// Lazily-constructed default backend. When no PlatformModule has booted
// we still want filesystem IO to work for callers like FileLogSink, so
// we fall back to the real native backend rather than the deny-all
// NullPlatformIO. NullPlatformIO remains the type-level default for
// IPlatformIO (returned when even the native factory fails) and is
// useful in tests that want to assert "no IO happened".
::gecko::Unique<IPlatformIO>& DefaultBackend() noexcept
{
  static ::gecko::Unique<IPlatformIO> instance = CreateNativePlatformIO();
  return instance;
}

NullPlatformIO s_NullPlatformIO;

}  // namespace

IPlatformIO* GetPlatformIO() noexcept
{
  if (auto* modules = ::gecko::GetModules())
  {
    if (auto* impl = modules->Service<IPlatformIO>())
      return impl;
  }
  if (auto& fallback = DefaultBackend(); fallback != nullptr)
    return fallback.get();
  return &s_NullPlatformIO;
}

}  // namespace gecko::platform
