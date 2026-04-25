#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

#include <string>
#include <string_view>

namespace gecko::platform {

// Forward-slash path view. Borrowed; the underlying string must outlive
// the PathView. All Gecko engine code passes paths around as PathView so
// the platform IO backends can normalize at syscall boundaries (Win32
// converts to backslashes / wide strings; Linux passes through).
//
// Construction is implicit from the common borrowed-string types so call
// sites read naturally:
//   io.Read("config/game.toml");
//   io.AtomicWrite(myStdString, bytes);
//
// Construction from std::filesystem::path is intentionally not provided;
// callers that have one must convert to a string explicitly so the
// forward-slash convention is preserved.
class PathView
{
public:
  constexpr PathView() noexcept = default;
  constexpr PathView(const char* str) noexcept
      : m_View(str == nullptr ? ::std::string_view {}
                              : ::std::string_view {str})
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

  // True if the path begins with '/' (POSIX-absolute) or with a Windows
  // drive letter ("C:/...") or UNC prefix ("//server/share/..."). The
  // Windows backend honours all three; the Linux backend only the first.
  [[nodiscard]] GECKO_API bool IsAbsolute() const noexcept;

  // Everything before the final '/' (excluding the slash). Returns an
  // empty view if there is no '/' or if the path is just "/".
  [[nodiscard]] GECKO_API PathView ParentDir() const noexcept;

  // Everything after the final '/'. Returns the whole view if there
  // is no '/'.
  [[nodiscard]] GECKO_API PathView Filename() const noexcept;

  // Filename minus its extension. Leading dots on the basename do not
  // count as an extension separator (".bashrc" -> ".bashrc", stem
  // "" — Stem returns the empty view).
  [[nodiscard]] GECKO_API PathView Stem() const noexcept;

  // Last '.'-suffix of the basename including the dot, or empty view if
  // no extension is present. Returns "" for ".bashrc" by convention.
  [[nodiscard]] GECKO_API PathView Extension() const noexcept;

  friend constexpr bool operator==(PathView a, PathView b) noexcept
  {
    return a.m_View == b.m_View;
  }

private:
  ::std::string_view m_View {};
};

}  // namespace gecko::platform
