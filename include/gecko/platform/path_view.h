#pragma once

/// @file
/// Borrowed forward-slash path string view used throughout Gecko I/O.
///
/// `PathView` is the Gecko-wide convention for passing filesystem paths.
/// Backends normalise at syscall boundaries (Win32 converts to
/// backslashes / wide strings; Linux passes through). Implicit
/// construction from `const char*` keeps literal call sites natural:
///
/// @code
/// io.Read("config/game.toml");
/// io.AtomicWrite(PathView {myString.Data(), myString.Size()}, bytes);
/// @endcode
///
/// Construction from `std::filesystem::path` is deliberately omitted so
/// that callers convert explicitly and preserve the forward-slash form.

#include "gecko/core/string_view.h"
#include "gecko/core/types.h"

namespace gecko::platform {

/// Borrowed forward-slash path. The underlying string buffer must
/// outlive every `PathView` referencing it.
class PathView
{
public:
  constexpr PathView() noexcept = default;
  constexpr PathView(const char* str) noexcept
      : m_View(str == nullptr ? ::gecko::StringView {} : ::gecko::StringView {str, Length(str)})
  {}
  constexpr PathView(::gecko::StringView view) noexcept : m_View(view)
  {}

  [[nodiscard]] constexpr ::gecko::StringView View() const noexcept
  {
    return m_View;
  }
  [[nodiscard]] constexpr const char* Data() const noexcept
  {
    return m_View.data();
  }
  [[nodiscard]] constexpr ::gecko::usize Size() const noexcept
  {
    return m_View.Size();
  }
  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_View.Empty();
  }

  /// `true` if the path begins with `/` (POSIX-absolute) or with a
  /// Windows drive letter (`C:/...`) or UNC prefix (`//server/share/...`).
  /// The Windows backend honours all three; the Linux backend only the first.
  [[nodiscard]] constexpr bool IsAbsolute() const noexcept
  {
    if (m_View.Empty())
      return false;
    if (m_View[0] == '/')
      return true;
    // Drive-letter form: "C:/...". Engine paths are forward-slash so we
    // do not check for backslash. The Win32 backend re-normalises before
    // syscalls.
    if (m_View.Size() >= 3 && m_View[1] == ':' && m_View[2] == '/')
    {
      char c = m_View[0];
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }
    return false;
  }

  /// Everything before the final `/` (excluding the slash). Returns an
  /// empty view if there is no `/`, or `"/"` if the path is just `"/"`.
  [[nodiscard]] constexpr PathView ParentDir() const noexcept
  {
    if (m_View.Empty())
      return PathView {};
    auto pos = FindLast(m_View, '/');
    if (pos == NPos)
      return PathView {};
    if (pos == 0)
      return PathView {"/"};
    return PathView {Substr(m_View, 0, pos)};
  }

  /// Everything after the final `/`. Returns the whole view if there
  /// is no `/`.
  [[nodiscard]] constexpr PathView Filename() const noexcept
  {
    if (m_View.Empty())
      return PathView {};
    auto pos = FindLast(m_View, '/');
    if (pos == NPos)
      return PathView {m_View};
    return PathView {Substr(m_View, pos + 1, m_View.Size() - (pos + 1))};
  }

  /// Filename minus its extension. Leading dots on the basename do not
  /// count as an extension separator (`.bashrc` -> empty stem).
  [[nodiscard]] constexpr PathView Stem() const noexcept
  {
    auto fn = Filename().View();
    if (fn.Empty())
      return PathView {};
    if (fn[0] == '.')
    {
      auto dot = FindLast(fn, '.');
      if (dot == 0)
        return PathView {};
      return PathView {Substr(fn, 0, dot)};
    }
    auto dot = FindLast(fn, '.');
    if (dot == NPos)
      return PathView {fn};
    return PathView {Substr(fn, 0, dot)};
  }

  /// Last `.`-suffix of the basename including the dot, or empty view
  /// if no extension is present. Returns `""` for `.bashrc`.
  [[nodiscard]] constexpr PathView Extension() const noexcept
  {
    auto fn = Filename().View();
    if (fn.Empty())
      return PathView {};
    auto dot = FindLast(fn, '.');
    if (dot == NPos || dot == 0)
      return PathView {};
    return PathView {Substr(fn, dot, fn.Size() - dot)};
  }

  friend constexpr bool operator==(PathView a, PathView b) noexcept
  {
    return a.m_View == b.m_View;
  }

private:
  static constexpr ::gecko::usize NPos = static_cast<::gecko::usize>(-1);

  [[nodiscard]] static constexpr ::gecko::usize Length(const char* str) noexcept
  {
    ::gecko::usize len = 0;
    while (str[len] != '\0')
      ++len;
    return len;
  }

  [[nodiscard]] static constexpr ::gecko::usize FindLast(::gecko::StringView view, char c) noexcept
  {
    for (::gecko::usize i = view.Size(); i > 0; --i)
    {
      if (view[i - 1] == c)
        return i - 1;
    }
    return NPos;
  }

  [[nodiscard]] static constexpr ::gecko::StringView Substr(::gecko::StringView view, ::gecko::usize offset,
                                                            ::gecko::usize count) noexcept
  {
    return {view.Data() + offset, count};
  }

  ::gecko::StringView m_View {};
};

}  // namespace gecko::platform
