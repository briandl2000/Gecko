#pragma once

/// @file
/// Borrowed forward-slash path string view used throughout Gecko I/O.
///
/// `PathView` is the Gecko-wide convention for passing filesystem paths.
/// Backends normalise at syscall boundaries (Win32 converts to
/// backslashes / wide strings; Linux passes through). Implicit
/// construction from `const char*`, `std::string_view`, and
/// `std::string` keeps call sites natural:
///
/// @code
/// io.Read("config/game.toml");
/// io.AtomicWrite(myStdString, bytes);
/// @endcode
///
/// Construction from `std::filesystem::path` is deliberately omitted so
/// that callers convert explicitly and preserve the forward-slash form.

#include "gecko/core/types.h"

#include <string>
#include <string_view>

namespace gecko::platform {

/// Borrowed forward-slash path. The underlying string buffer must
/// outlive every `PathView` referencing it.
class PathView
{
public:
  constexpr PathView() noexcept = default;
  constexpr PathView(const char* str) noexcept
      : m_View(str == nullptr ? ::std::string_view {} : ::std::string_view {str})
  {}
  constexpr PathView(::std::string_view view) noexcept : m_View(view)
  {}
  PathView(const ::std::string& s) noexcept : m_View(s)
  {}

  [[nodiscard]] constexpr ::std::string_view View() const noexcept
  {
    return m_View;
  }
  [[nodiscard]] constexpr const char* Data() const noexcept
  {
    return m_View.data();
  }
  [[nodiscard]] constexpr ::std::size_t Size() const noexcept
  {
    return m_View.size();
  }
  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_View.empty();
  }

  /// `true` if the path begins with `/` (POSIX-absolute) or with a
  /// Windows drive letter (`C:/...`) or UNC prefix (`//server/share/...`).
  /// The Windows backend honours all three; the Linux backend only the first.
  [[nodiscard]] constexpr bool IsAbsolute() const noexcept
  {
    if (m_View.empty())
      return false;
    if (m_View.front() == '/')
      return true;
    // Drive-letter form: "C:/...". Engine paths are forward-slash so we
    // do not check for backslash. The Win32 backend re-normalises before
    // syscalls.
    if (m_View.size() >= 3 && m_View[1] == ':' && m_View[2] == '/')
    {
      char c = m_View.front();
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }
    return false;
  }

  /// Everything before the final `/` (excluding the slash). Returns an
  /// empty view if there is no `/`, or `"/"` if the path is just `"/"`.
  [[nodiscard]] constexpr PathView ParentDir() const noexcept
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

  /// Everything after the final `/`. Returns the whole view if there
  /// is no `/`.
  [[nodiscard]] constexpr PathView Filename() const noexcept
  {
    if (m_View.empty())
      return PathView {};
    auto pos = m_View.find_last_of('/');
    if (pos == ::std::string_view::npos)
      return PathView {m_View};
    return PathView {m_View.substr(pos + 1)};
  }

  /// Filename minus its extension. Leading dots on the basename do not
  /// count as an extension separator (`.bashrc` -> empty stem).
  [[nodiscard]] constexpr PathView Stem() const noexcept
  {
    auto fn = Filename().View();
    if (fn.empty())
      return PathView {};
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

  /// Last `.`-suffix of the basename including the dot, or empty view
  /// if no extension is present. Returns `""` for `.bashrc`.
  [[nodiscard]] constexpr PathView Extension() const noexcept
  {
    auto fn = Filename().View();
    if (fn.empty())
      return PathView {};
    auto dot = fn.find_last_of('.');
    if (dot == ::std::string_view::npos || dot == 0)
      return PathView {};
    return PathView {fn.substr(dot)};
  }

  friend constexpr bool operator==(PathView a, PathView b) noexcept
  {
    return a.m_View == b.m_View;
  }

private:
  ::std::string_view m_View {};
};

}  // namespace gecko::platform
