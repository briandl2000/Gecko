#if defined(GECKO_PLATFORM_LINUX)

#include "platform_io_linux.h"

#include "gecko/core/services/memory.h"
#include "gecko/platform/platform_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <new>

namespace gecko::platform {

namespace linux_io {

String ToCString(PathView path) noexcept
{
  return String {path.View()};
}

}  // namespace linux_io

namespace {

using linux_io::ToCString;

struct LinuxMapHandle
{
  void* Address {nullptr};
  usize Size {0};
  int File {-1};
};

void UnmapLinux(void* opaque) noexcept
{
  auto* handle = static_cast<LinuxMapHandle*>(opaque);
  if (handle == nullptr)
    return;
  if (handle->Address != nullptr && handle->Size != 0)
    (void)::munmap(handle->Address, handle->Size);
  if (handle->File >= 0)
    (void)::close(handle->File);
  handle->~LinuxMapHandle();
  DeallocBytes(handle);
}

bool DirIterNext(void* handle, DirEntry* output) noexcept
{
  auto* directory = static_cast<DIR*>(handle);
  for (;;)
  {
    errno = 0;
    dirent* entry = ::readdir(directory);
    if (entry == nullptr)
      return false;
    if (entry->d_name[0] == '.' &&
        (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
      continue;
    output->Name.Assign(StringView {entry->d_name});
    output->IsDirectory = entry->d_type == DT_DIR;
    return true;
  }
}

void DirIterClose(void* handle) noexcept
{
  if (handle != nullptr)
    (void)::closedir(static_cast<DIR*>(handle));
}

class LinuxFileWriter final : public FileWriter
{
public:
  explicit LinuxFileWriter(int file) noexcept : m_File(file)
  {}
  ~LinuxFileWriter() noexcept override
  {
    if (m_File >= 0)
      (void)::close(m_File);
  }

  bool Write(Span<const byte> data) noexcept override
  {
    usize total = 0;
    while (m_File >= 0 && total < data.Count())
    {
      const ssize_t count = ::write(m_File, data.Data() + total, data.Count() - total);
      if (count < 0)
      {
        if (errno == EINTR)
          continue;
        return false;
      }
      total += static_cast<usize>(count);
    }
    return total == data.Count();
  }
  bool Flush() noexcept override
  {
    return m_File >= 0;
  }
  u64 Seek(i64 offset, bool fromEnd) noexcept override
  {
    const off_t result = m_File >= 0 ? ::lseek(m_File, static_cast<off_t>(offset), fromEnd ? SEEK_END : SEEK_SET) : -1;
    return result >= 0 ? static_cast<u64>(result) : U64Max;
  }
  u64 Tell() noexcept override
  {
    const off_t result = m_File >= 0 ? ::lseek(m_File, 0, SEEK_CUR) : -1;
    return result >= 0 ? static_cast<u64>(result) : U64Max;
  }

private:
  int m_File {-1};
};

bool WriteFileContents(int file, Span<const byte> data) noexcept
{
  usize total = 0;
  while (total < data.Count())
  {
    const ssize_t count = ::write(file, data.Data() + total, data.Count() - total);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      return false;
    }
    total += static_cast<usize>(count);
  }
  return true;
}

}  // namespace

bool Exists(PathView path) noexcept
{
  const String native = ToCString(path);
  return ::access(native.CStr(), F_OK) == 0;
}

Optional<FileStat> Stat(PathView path) noexcept
{
  const String native = ToCString(path);
  struct stat info {};
  if (::stat(native.CStr(), &info) != 0)
    return {};
  return FileStat {
      .Size = static_cast<u64>(info.st_size),
      .MTimeEpoch = static_cast<i64>(info.st_mtime),
      .IsDirectory = S_ISDIR(info.st_mode),
  };
}

ReadResult Read(PathView path) noexcept
{
  const String native = ToCString(path);
  const int file = ::open(native.CStr(), O_RDONLY | O_CLOEXEC);
  if (file < 0)
    return {};
  struct stat info {};
  if (::fstat(file, &info) != 0 || info.st_size < 0)
  {
    (void)::close(file);
    return {};
  }

  Array<byte> bytes(static_cast<usize>(info.st_size));
  usize total = 0;
  while (total < bytes.Count())
  {
    const ssize_t count = ::read(file, bytes.Data() + total, bytes.Count() - total);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      (void)::close(file);
      return {};
    }
    if (count == 0)
      break;
    total += static_cast<usize>(count);
  }
  (void)::close(file);
  bytes.Resize(total);
  return ReadResult {Move(bytes)};
}

MappedFile Map(PathView path) noexcept
{
  const String native = ToCString(path);
  const int file = ::open(native.CStr(), O_RDONLY | O_CLOEXEC);
  if (file < 0)
    return {};
  struct stat info {};
  if (::fstat(file, &info) != 0 || info.st_size <= 0)
  {
    (void)::close(file);
    return {};
  }
  const usize size = static_cast<usize>(info.st_size);
  void* address = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, file, 0);
  if (address == MAP_FAILED)
  {
    (void)::close(file);
    return {};
  }
  void* storage = AllocBytes(sizeof(LinuxMapHandle), alignof(LinuxMapHandle));
  auto* handle = new (storage) LinuxMapHandle {.Address = address, .Size = size, .File = file};
  return MappedFile {static_cast<const byte*>(address), size, handle, &UnmapLinux};
}

WriteResult Write(PathView path, Span<const byte> data, WriteMode mode) noexcept
{
  const String native = ToCString(path);
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
  flags |= mode == WriteMode::Append ? O_APPEND : O_TRUNC;
  const int file = ::open(native.CStr(), flags, 0644);
  if (file < 0)
    return {};
  const bool ok = WriteFileContents(file, data);
  (void)::close(file);
  return WriteResult {.Ok = ok, .BytesWritten = ok ? static_cast<u64>(data.Count()) : 0};
}

bool AtomicWrite(PathView path, Span<const byte> data) noexcept
{
  const String target = ToCString(path);
  String temporary = target;
  temporary.Append(".tmp");
  const int file = ::open(temporary.CStr(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (file < 0)
    return false;
  const bool written = WriteFileContents(file, data);
  const bool synced = written && ::fsync(file) == 0;
  (void)::close(file);
  const long renamed = synced ? ::syscall(SYS_renameat, AT_FDCWD, temporary.CStr(), AT_FDCWD, target.CStr()) : -1;
  if (renamed != 0)
  {
    (void)::unlink(temporary.CStr());
    return false;
  }
  return true;
}

Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept
{
  const String native = ToCString(path);
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
  if (mode == WriteMode::Truncate)
    flags |= O_TRUNC;
  const int file = ::open(native.CStr(), flags, 0644);
  if (file < 0)
    return {};
  if (mode == WriteMode::Append)
    (void)::lseek(file, 0, SEEK_END);
  return CreateUnique<LinuxFileWriter>(file);
}

bool CreateDir(PathView path, bool recursive) noexcept
{
  const String native = ToCString(path);
  if (native.Empty())
    return false;
  if (!recursive)
    return ::mkdir(native.CStr(), 0755) == 0 || errno == EEXIST;

  const StringView full = native.View();
  for (usize index = 1; index <= full.Size(); ++index)
  {
    if (index != full.Size() && full[index] != '/')
      continue;
    const StringView part = full.Substring(0, index);
    if (part.Empty() || part == StringView {"/"})
      continue;
    const String directory(part);
    if (::mkdir(directory.CStr(), 0755) != 0 && errno != EEXIST)
      return false;
  }
  return true;
}

bool Remove(PathView path) noexcept
{
  const String native = ToCString(path);
  return ::unlink(native.CStr()) == 0 || ::rmdir(native.CStr()) == 0;
}

DirIter IterateDir(PathView path) noexcept
{
  const String native = ToCString(path);
  DIR* directory = ::opendir(native.CStr());
  return directory != nullptr ? DirIter {directory, DirIterNext, DirIterClose} : DirIter {};
}

String ExePath() noexcept
{
  char buffer[PATH_MAX];
  const ssize_t count = ::readlink("/proc/self/exe", buffer, sizeof(buffer));
  return count > 0 ? String {StringView {buffer, static_cast<usize>(count)}} : String {};
}

String WorkingDir() noexcept
{
  char buffer[PATH_MAX];
  return ::getcwd(buffer, sizeof(buffer)) != nullptr ? String {buffer} : String {};
}

String UserDataDir(StringView appName) noexcept
{
  String path;
  const char* xdg = ::getenv("XDG_DATA_HOME");
  if (xdg != nullptr && xdg[0] != '\0')
  {
    path.Assign(xdg);
  }
  else
  {
    const char* home = ::getenv("HOME");
    if (home == nullptr || home[0] == '\0')
      return {};
    path.Assign(home);
    path.Append("/.local/share");
  }
  if (!appName.Empty())
  {
    path.Append('/');
    path.Append(appName);
  }
  return path;
}

}  // namespace gecko::platform

#endif
