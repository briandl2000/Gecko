#include "gecko/platform/platform_io.h"

namespace gecko::platform {

ReadResult::ReadResult(Array<byte> bytes) noexcept : m_Bytes(Move(bytes)), m_Ok(true)
{}

ReadResult::ReadResult(ReadResult&& other) noexcept : m_Bytes(Move(other.m_Bytes)), m_Ok(other.m_Ok)
{
  other.m_Ok = false;
}

ReadResult& ReadResult::operator=(ReadResult&& other) noexcept
{
  if (this != &other)
  {
    m_Bytes = Move(other.m_Bytes);
    m_Ok = other.m_Ok;
    other.m_Ok = false;
  }
  return *this;
}

Array<byte> ReadResult::Take() noexcept
{
  m_Ok = false;
  return Move(m_Bytes);
}

MappedFile::MappedFile(const byte* data, usize size, void* handle, Deleter deleter) noexcept
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
  if (m_Deleter != nullptr && m_Handle != nullptr)
    m_Deleter(m_Handle);
  m_Data = nullptr;
  m_Size = 0;
  m_Handle = nullptr;
  m_Deleter = nullptr;
}

DirIter::DirIter(void* handle, NextFn next, CloseFn close) noexcept
    : m_Handle(handle), m_Next(next), m_Close(close)
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

Optional<DirEntry> DirIter::Next() noexcept
{
  DirEntry entry;
  if (m_Handle == nullptr || m_Next == nullptr || !m_Next(m_Handle, &entry))
    return {};
  return Optional<DirEntry> {Move(entry)};
}

void DirIter::Close() noexcept
{
  if (m_Close != nullptr && m_Handle != nullptr)
    m_Close(m_Handle);
  m_Handle = nullptr;
  m_Next = nullptr;
  m_Close = nullptr;
}

bool FileWriter::WriteString(StringView text) noexcept
{
  return Write(Span<const byte> {reinterpret_cast<const byte*>(text.Data()), text.Size()});
}

}  // namespace gecko::platform
