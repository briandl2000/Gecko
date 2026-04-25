#if defined(GECKO_PLATFORM_LINUX)

#include "../private/native_platform_io.h"
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

namespace {

// Convert a PathView to a NUL-terminated heap buffer suitable for
// passing to syscalls. Returns empty string if the input is empty.
::std::string ToCString(PathView path) noexcept
{
  return ::std::string {path.View()};
}

class LinuxPlatformIO final : public IPlatformIO
{
public:
  bool Exists(PathView path) noexcept override
  {
    auto p = ToCString(path);
    return ::access(p.c_str(), F_OK) == 0;
  }

  ::std::optional<FileStat> Stat(PathView path) noexcept override
  {
    auto p = ToCString(path);
    struct stat st {};
    if (::stat(p.c_str(), &st) != 0)
      return ::std::nullopt;
    FileStat fs {};
    fs.Size = static_cast<::gecko::u64>(st.st_size);
    fs.MTimeEpoch = static_cast<::gecko::i64>(st.st_mtime);
    fs.IsDirectory = S_ISDIR(st.st_mode);
    return fs;
  }

  ReadResult Read(PathView path) noexcept override
  {
    auto p = ToCString(path);
    int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
      return {};

    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
    {
      ::close(fd);
      return {};
    }

    ::std::vector<::std::byte> buf;
    auto size = static_cast<::std::size_t>(st.st_size);
    try
    {
      buf.resize(size);
    }
    catch (...)
    {
      ::close(fd);
      return {};
    }

    ::std::size_t total = 0;
    while (total < size)
    {
      ssize_t n = ::read(fd, buf.data() + total, size - total);
      if (n < 0)
      {
        if (errno == EINTR)
          continue;
        ::close(fd);
        return {};
      }
      if (n == 0)  // unexpected EOF; truncate
      {
        buf.resize(total);
        break;
      }
      total += static_cast<::std::size_t>(n);
    }
    ::close(fd);
    return ReadResult {::std::move(buf)};
  }

  MappedFile Map(PathView path) noexcept override
  {
    auto p = ToCString(path);
    int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
      return {};

    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size == 0)
    {
      ::close(fd);
      return {};
    }

    auto size = static_cast<::std::size_t>(st.st_size);
    void* addr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);  // safe to close fd after mmap; mapping holds its own ref
    if (addr == MAP_FAILED)
      return {};

    // Pack (addr, size) into a single heap object so the deleter has a
    // single pointer to free.
    struct Mapping
    {
      void* Addr;
      ::std::size_t Size;
    };
    auto* m = new (::std::nothrow) Mapping {addr, size};
    if (m == nullptr)
    {
      ::munmap(addr, size);
      return {};
    }

    auto deleter = [](void* handle) noexcept {
      auto* mapping = static_cast<Mapping*>(handle);
      ::munmap(mapping->Addr, mapping->Size);
      delete mapping;
    };

    return MappedFile {static_cast<const ::std::byte*>(addr), size, m, deleter};
  }

  WriteResult Write(PathView path, ::std::span<const ::std::byte> data,
                    WriteMode mode) noexcept override
  {
    auto p = ToCString(path);
    int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
    flags |= (mode == WriteMode::Truncate) ? O_TRUNC : O_APPEND;
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
    return WriteResult {.Ok = true,
                        .BytesWritten = static_cast<::gecko::u64>(total)};
  }

  bool AtomicWrite(PathView path,
                   ::std::span<const ::std::byte> data) noexcept override
  {
    auto target = ToCString(path);
    auto tmp = target + ".tmp";

    int fd =
        ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
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

  bool CreateDir(PathView path, bool recursive) noexcept override
  {
    auto p = ToCString(path);
    if (p.empty())
      return false;

    if (!recursive)
      return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;

    // Walk the path, creating each parent in turn. mkdir is happy to
    // fail with EEXIST; everything else is a real error.
    for (::std::size_t i = 1; i <= p.size(); ++i)
    {
      if (i == p.size() || p[i] == '/')
      {
        char saved = (i < p.size()) ? p[i] : '\0';
        p[i] = '\0';
        if (p[0] != '\0' && ::mkdir(p.c_str(), 0755) != 0 && errno != EEXIST)
        {
          if (i < p.size())
            p[i] = saved;
          return false;
        }
        if (i < p.size())
          p[i] = saved;
      }
    }
    return true;
  }

  bool Remove(PathView path) noexcept override
  {
    auto p = ToCString(path);
    // Try unlink first (file); fall back to rmdir on EISDIR.
    if (::unlink(p.c_str()) == 0)
      return true;
    if (errno == EISDIR)
      return ::rmdir(p.c_str()) == 0;
    return false;
  }

  DirIter IterateDir(PathView path) noexcept override
  {
    auto p = ToCString(path);
    DIR* d = ::opendir(p.c_str());
    if (d == nullptr)
      return {};

    auto next = [](void* handle, DirEntry& out) noexcept -> bool {
      auto* dir = static_cast<DIR*>(handle);
      for (;;)
      {
        errno = 0;
        struct dirent* e = ::readdir(dir);
        if (e == nullptr)
          return false;
        if (::strcmp(e->d_name, ".") == 0 || ::strcmp(e->d_name, "..") == 0)
          continue;
        try
        {
          out.Name.assign(e->d_name);
        }
        catch (...)
        {
          return false;
        }
        out.IsDirectory = (e->d_type == DT_DIR);
        return true;
      }
    };
    auto close = [](void* handle) noexcept {
      ::closedir(static_cast<DIR*>(handle));
    };
    return DirIter {d, next, close};
  }

  ::std::string ExePath() noexcept override
  {
    char buf[PATH_MAX];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf));
    if (n <= 0)
      return {};
    try
    {
      return ::std::string {buf, static_cast<::std::size_t>(n)};
    }
    catch (...)
    {
      return {};
    }
  }

  ::std::string WorkingDir() noexcept override
  {
    char buf[PATH_MAX];
    if (::getcwd(buf, sizeof(buf)) == nullptr)
      return {};
    try
    {
      return ::std::string {buf};
    }
    catch (...)
    {
      return {};
    }
  }

  ::std::string UserDataDir(::std::string_view appName) noexcept override
  {
    // XDG: $XDG_DATA_HOME or $HOME/.local/share, then append /<appName>.
    const char* xdg = ::getenv("XDG_DATA_HOME");
    ::std::string base;
    try
    {
      if (xdg != nullptr && xdg[0] != '\0')
      {
        base.assign(xdg);
      }
      else
      {
        const char* home = ::getenv("HOME");
        if (home == nullptr || home[0] == '\0')
          return {};
        base.assign(home);
        base.append("/.local/share");
      }
      if (!appName.empty())
      {
        base.push_back('/');
        base.append(appName);
      }
    }
    catch (...)
    {
      return {};
    }
    return base;
  }
};

}  // namespace

::gecko::Unique<IPlatformIO> CreateNativePlatformIO() noexcept
{
  return ::gecko::CreateUnique<LinuxPlatformIO>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX
