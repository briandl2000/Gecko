#if defined(GECKO_PLATFORM_LINUX)

#include "platform_io_linux.h"

#include "gecko/core/ptr.h"
#include "gecko/platform/path_view.h"
#include "gecko/platform/platform_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace gecko::platform {

namespace linux_io {

::std::string ToCString(PathView path) noexcept
{
  auto view = path.View();
  return ::std::string {view.Data(), view.Size()};
}

}  // namespace linux_io

namespace {

using linux_io::ToCString;

// Map deleter for MappedFile. Frees the (size, addr) pair stored in
// the heap handle and then closes the underlying fd.
struct LinuxMapHandle
{
  void* Address;
  ::std::size_t Size;
  int Fd;
};

void UnmapLinux(void* opaque) noexcept
{
  auto* h = static_cast<LinuxMapHandle*>(opaque);
  if (!h)
    return;
  if (h->Address && h->Size > 0)
    ::munmap(h->Address, h->Size);
  if (h->Fd >= 0)
    ::close(h->Fd);
  delete h;
}

bool DirIterNext(void* handle, DirEntry* out) noexcept
{
  auto* d = static_cast<DIR*>(handle);
  while (true)
  {
    errno = 0;
    struct dirent* e = ::readdir(d);
    if (!e)
      return false;
    if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
      continue;
    try
    {
      if (!out->Name.Assign(::gecko::StringView {e->d_name, ::std::strlen(e->d_name)}))
        return false;
    }
    catch (...)
    {
      return false;
    }
    out->IsDirectory = (e->d_type == DT_DIR);
    return true;
  }
}

void DirIterClose(void* handle) noexcept
{
  if (handle)
    ::closedir(static_cast<DIR*>(handle));
}

class LinuxFileWriter final : public FileWriter
{
public:
  explicit LinuxFileWriter(int fd) noexcept : m_Fd(fd)
  {}

  ~LinuxFileWriter() noexcept override
  {
    if (m_Fd >= 0)
      ::close(m_Fd);
  }

  bool Write(::gecko::ConstByteSpan data) noexcept override
  {
    if (m_Fd < 0)
      return false;
    ::std::size_t total = 0;
    while (total < data.size())
    {
      ssize_t n = ::write(m_Fd, data.data() + total, data.size() - total);
      if (n < 0)
      {
        if (errno == EINTR)
          continue;
        return false;
      }
      total += static_cast<::std::size_t>(n);
    }
    return true;
  }

  bool Flush() noexcept override
  {
    return m_Fd >= 0;
  }

  ::gecko::u64 Seek(::gecko::i64 offset, bool fromEnd) noexcept override
  {
    if (m_Fd < 0)
      return static_cast<::gecko::u64>(-1);
    off_t r = ::lseek(m_Fd, static_cast<off_t>(offset), fromEnd ? SEEK_END : SEEK_SET);
    if (r < 0)
      return static_cast<::gecko::u64>(-1);
    return static_cast<::gecko::u64>(r);
  }

  ::gecko::u64 Tell() noexcept override
  {
    if (m_Fd < 0)
      return static_cast<::gecko::u64>(-1);
    off_t r = ::lseek(m_Fd, 0, SEEK_CUR);
    if (r < 0)
      return static_cast<::gecko::u64>(-1);
    return static_cast<::gecko::u64>(r);
  }

private:
  int m_Fd {-1};
};

}  // namespace

// -- Public free-function impls --------------------------------------

bool Exists(PathView path) noexcept
{
  auto p = ToCString(path);
  return ::access(p.c_str(), F_OK) == 0;
}

StatResult Stat(PathView path) noexcept
{
  auto p = ToCString(path);
  struct stat st {};
  if (::stat(p.c_str(), &st) != 0)
    return {};
  FileStat fs {};
  fs.Size = static_cast<::gecko::u64>(st.st_size);
  fs.MTimeEpoch = static_cast<::gecko::i64>(st.st_mtime);
  fs.IsDirectory = S_ISDIR(st.st_mode);
  return fs;
}

ReadResult Read(PathView path) noexcept
{
  auto p = ToCString(path);
  int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return {};

  struct stat st {};
  if (::fstat(fd, &st) != 0)
  {
    ::close(fd);
    return {};
  }

  ::gecko::Array<::gecko::byte> bytes;
  if (!bytes.Resize(static_cast<::gecko::usize>(st.st_size)))
  {
    ::close(fd);
    return {};
  }

  ::gecko::usize total = 0;
  while (total < bytes.Count())
  {
    ssize_t n = ::read(fd, bytes.Data() + total, bytes.Count() - total);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      ::close(fd);
      return {};
    }
    if (n == 0)
      break;
    total += static_cast<::gecko::usize>(n);
  }
  ::close(fd);
  (void)bytes.Resize(total);
  return ReadResult {::std::move(bytes)};
}

MappedFile Map(PathView path) noexcept
{
  auto p = ToCString(path);
  int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return {};

  struct stat st {};
  if (::fstat(fd, &st) != 0 || st.st_size == 0)
  {
    ::close(fd);
    return {};
  }
  auto size = static_cast<::std::size_t>(st.st_size);
  void* addr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED)
  {
    ::close(fd);
    return {};
  }
  auto* h = new (::std::nothrow) LinuxMapHandle {addr, size, fd};
  if (!h)
  {
    ::munmap(addr, size);
    ::close(fd);
    return {};
  }
  return MappedFile {static_cast<const ::gecko::byte*>(addr), size, h, &UnmapLinux};
}

WriteResult Write(PathView path, ::gecko::ConstByteSpan data, WriteMode mode) noexcept
{
  auto p = ToCString(path);
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
  flags |= (mode == WriteMode::Append) ? O_APPEND : O_TRUNC;

  int fd = ::open(p.c_str(), flags, 0644);
  if (fd < 0)
    return {};

  ::std::size_t total = 0;
  while (total < data.size())
  {
    ssize_t n = ::write(fd, data.data() + total, data.size() - total);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      ::close(fd);
      return {};
    }
    total += static_cast<::std::size_t>(n);
  }
  ::close(fd);
  return WriteResult {.Ok = true, .BytesWritten = static_cast<::gecko::u64>(total)};
}

bool AtomicWrite(PathView path, ::gecko::ConstByteSpan data) noexcept
{
  auto target = ToCString(path);
  auto tmp = target + ".tmp";

  int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0)
    return false;

  ::std::size_t total = 0;
  while (total < data.size())
  {
    ssize_t n = ::write(fd, data.data() + total, data.size() - total);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      ::close(fd);
      ::unlink(tmp.c_str());
      return false;
    }
    total += static_cast<::std::size_t>(n);
  }

  if (::fsync(fd) != 0)
  {
    ::close(fd);
    ::unlink(tmp.c_str());
    return false;
  }
  ::close(fd);

  if (::rename(tmp.c_str(), target.c_str()) != 0)
  {
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

::gecko::Unique<FileWriter> OpenWrite(PathView path, WriteMode mode) noexcept
{
  auto p = ToCString(path);
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
  if (mode == WriteMode::Truncate)
    flags |= O_TRUNC;
  int fd = ::open(p.c_str(), flags, 0644);
  if (fd < 0)
    return {};
  if (mode == WriteMode::Append)
    ::lseek(fd, 0, SEEK_END);
  return ::gecko::CreateUnique<LinuxFileWriter>(fd);
}

bool CreateDir(PathView path, bool recursive) noexcept
{
  auto p = ToCString(path);
  if (p.empty())
    return false;

  if (!recursive)
    return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;

  for (::std::size_t i = 1; i <= p.size(); ++i)
  {
    if (i == p.size() || p[i] == '/')
    {
      ::std::string sub(p, 0, i);
      if (sub.empty() || sub == "/")
        continue;
      if (::mkdir(sub.c_str(), 0755) != 0 && errno != EEXIST)
        return false;
    }
  }
  return true;
}

bool Remove(PathView path) noexcept
{
  auto p = ToCString(path);
  if (::unlink(p.c_str()) == 0)
    return true;
  if (::rmdir(p.c_str()) == 0)
    return true;
  return false;
}

DirIter IterateDir(PathView path) noexcept
{
  auto p = ToCString(path);
  DIR* d = ::opendir(p.c_str());
  if (!d)
    return {};
  return DirIter {d, &DirIterNext, &DirIterClose};
}

::gecko::String ExePath() noexcept
{
  char buf[PATH_MAX];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf));
  if (n <= 0)
    return {};
  return ::gecko::String {::gecko::StringView {buf, static_cast<::gecko::usize>(n)}};
}

::gecko::String WorkingDir() noexcept
{
  char buf[PATH_MAX];
  if (!::getcwd(buf, sizeof(buf)))
    return {};
  return ::gecko::String {::gecko::StringView {buf, ::std::strlen(buf)}};
}

::gecko::String UserDataDir(::gecko::StringView appName) noexcept
{
  ::std::string base;
  try
  {
    if (const char* xdg = ::getenv("XDG_DATA_HOME"); xdg && xdg[0] != '\0')
    {
      base = xdg;
    }
    else
    {
      const char* home = ::getenv("HOME");
      if (!home || home[0] == '\0')
        return {};
      base = home;
      base.append("/.local/share");
    }
    if (!appName.empty())
    {
      base.push_back('/');
      base.append(appName.Data(), appName.Size());
    }
  }
  catch (...)
  {
    return {};
  }
  return ::gecko::String {::gecko::StringView {base.data(), base.size()}};
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX
