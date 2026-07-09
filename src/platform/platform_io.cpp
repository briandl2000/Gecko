#include "gecko/platform/platform_io.h"

#include <utility>

namespace gecko::platform {

// -- ReadResult ------------------------------------------------------

ReadResult::ReadResult(::gecko::Array<::gecko::byte> bytes) noexcept : m_Bytes(::std::move(bytes)), m_Ok(true)
{}

ReadResult::ReadResult(ReadResult&& other) noexcept : m_Bytes(::std::move(other.m_Bytes)), m_Ok(other.m_Ok)
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

::gecko::Array<::gecko::byte> ReadResult::Take() noexcept
{
  m_Ok = false;
  return ::std::move(m_Bytes);
}

// -- MappedFile ------------------------------------------------------

MappedFile::MappedFile(const ::std::byte* data, ::std::size_t size, void* handle, Deleter deleter) noexcept
    : m_Data(data), m_Size(size), m_Handle(handle), m_Deleter(deleter)
{}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : m_Data(other.m_Data), m_Size(other.m_Size), m_Handle(other.m_Handle), m_Deleter(other.m_Deleter)
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

// -- DirIter ---------------------------------------------------------

DirIter::DirIter(void* handle, NextFn nextFn, CloseFn closeFn) noexcept
    : m_Handle(handle), m_Next(nextFn), m_Close(closeFn)
{}

DirIter::DirIter(DirIter&& other) noexcept : m_Handle(other.m_Handle), m_Next(other.m_Next), m_Close(other.m_Close)
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

// -- FileWriter helpers ----------------------------------------------

bool FileWriter::WriteString(::gecko::StringView text) noexcept
{
  return Write({reinterpret_cast<const ::gecko::byte*>(text.Data()), text.Size()});
}

}  // namespace gecko::platform
