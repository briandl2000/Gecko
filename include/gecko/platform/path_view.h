#pragma once

#include "gecko/core/string.h"

namespace gecko::platform {

class PathView
{
public:
  constexpr PathView() noexcept = default;
  constexpr PathView(const char* text) noexcept : m_View(text)
  {}
  constexpr PathView(StringView view) noexcept : m_View(view)
  {}
  PathView(const String& string) noexcept : m_View(string.View())
  {}

  [[nodiscard]] constexpr StringView View() const noexcept
  {
    return m_View;
  }
  [[nodiscard]] constexpr const char* Data() const noexcept
  {
    return m_View.Data();
  }
  [[nodiscard]] constexpr usize Size() const noexcept
  {
    return m_View.Size();
  }
  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_View.Empty();
  }

  [[nodiscard]] constexpr bool IsAbsolute() const noexcept
  {
    if (m_View.Empty())
      return false;
    if (m_View[0] == '/')
      return true;
    if (m_View.Size() >= 3 && m_View[1] == ':' && m_View[2] == '/')
    {
      const char character = m_View[0];
      return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    }
    return false;
  }

  [[nodiscard]] constexpr PathView ParentDir() const noexcept
  {
    if (m_View.Empty())
      return {};
    const usize position = m_View.FindLast('/');
    if (position == StringView::NotFound)
      return {};
    if (position == 0)
      return PathView {StringView {"/"}};
    return PathView {m_View.Substring(0, position)};
  }

  [[nodiscard]] constexpr PathView Filename() const noexcept
  {
    const usize position = m_View.FindLast('/');
    return position == StringView::NotFound ? *this : PathView {m_View.Substring(position + 1U)};
  }

  [[nodiscard]] constexpr PathView Stem() const noexcept
  {
    const StringView filename = Filename().View();
    if (filename.Empty())
      return {};
    const usize dot = filename.FindLast('.');
    if (dot == StringView::NotFound)
      return PathView {filename};
    if (dot == 0)
      return {};
    return PathView {filename.Substring(0, dot)};
  }

  [[nodiscard]] constexpr PathView Extension() const noexcept
  {
    const StringView filename = Filename().View();
    const usize dot = filename.FindLast('.');
    if (dot == StringView::NotFound || dot == 0)
      return {};
    return PathView {filename.Substring(dot)};
  }

  friend constexpr bool operator==(PathView first, PathView second) noexcept
  {
    return first.m_View == second.m_View;
  }

private:
  StringView m_View {};
};

}  // namespace gecko::platform
