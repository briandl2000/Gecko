#include "gecko/platform/platform_io.h"

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
  if (m_View.empty())
    return PathView {};
  auto pos = m_View.find_last_of('/');
  if (pos == ::std::string_view::npos)
    return PathView {};
  if (pos == 0)
    return PathView {::std::string_view {"/"}};
  return PathView {m_View.substr(0, pos)};
}

PathView PathView::Filename() const noexcept
{
  if (m_View.empty())
    return PathView {};
  auto pos = m_View.find_last_of('/');
  if (pos == ::std::string_view::npos)
    return PathView {m_View};
  return PathView {m_View.substr(pos + 1)};
}

PathView PathView::Stem() const noexcept
{
  auto fn = Filename().View();
  if (fn.empty())
    return PathView {};
  // Leading '.' on dotfiles (".bashrc") is part of the basename, not a
  // separator. Treat a leading '.' as no extension and return empty
  // stem to match the header contract.
  if (fn.front() == '.')
  {
    auto dot = fn.find_last_of('.');
    if (dot == 0)
      return PathView {};
    return PathView {fn.substr(0, dot)};
  }
  auto dot = fn.find_last_of('.');
  if (dot == ::std::string_view::npos)
    return PathView {fn};
  return PathView {fn.substr(0, dot)};
}

PathView PathView::Extension() const noexcept
{
  auto fn = Filename().View();
  if (fn.empty())
    return PathView {};
  auto dot = fn.find_last_of('.');
  if (dot == ::std::string_view::npos || dot == 0)
    return PathView {};
  return PathView {fn.substr(dot)};
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

void MappedFile::Reset() noexcept
{
  if (m_Deleter && m_Handle)
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

::std::optional<DirEntry> DirIter::Next() noexcept
{
  if (!m_Handle || !m_Next)
    return ::std::nullopt;
  DirEntry entry;
  if (!m_Next(m_Handle, &entry))
    return ::std::nullopt;
  return entry;
}

void DirIter::Close() noexcept
{
  if (m_Close && m_Handle)
    m_Close(m_Handle);
  m_Handle = nullptr;
  m_Next = nullptr;
  m_Close = nullptr;
}

// ── FileWriter helpers ──────────────────────────────────────────────

bool FileWriter::WriteString(::std::string_view text) noexcept
{
  return Write(
      {reinterpret_cast<const ::std::byte*>(text.data()), text.size()});
}

}  // namespace gecko::platform
