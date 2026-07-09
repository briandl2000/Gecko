#pragma once

/// @file
/// Formatting helpers that return/append Gecko-owned strings.
///
/// This is intentionally an opt-in header: compile-time checked
/// formatting uses `std::format_string`, so consumers including this
/// header also include the standard formatting machinery.

#include "gecko/core/string.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace gecko {

template <typename... Args>
using FormatString = ::std::format_string<Args...>;

struct RuntimeFormatString
{
  StringView Value {};
};

[[nodiscard]] constexpr RuntimeFormatString RuntimeFormat(StringView value) noexcept
{
  return RuntimeFormatString {value};
}

namespace detail {

[[nodiscard]] inline ::std::string_view ToStdStringView(StringView view) noexcept
{
  return {view.Data(), view.Size()};
}

[[nodiscard]] inline IAllocator& ResolveFormatAllocator(IAllocator* allocator) noexcept
{
  return allocator == nullptr ? CurrentAllocator() : *allocator;
}

[[nodiscard]] inline String StringFromStd(IAllocator& allocator, const ::std::string& text) noexcept
{
  String out {allocator};
  (void)out.Append({text.data(), text.size()});
  return out;
}

}  // namespace detail

template <typename... Args>
[[nodiscard]] String Format(IAllocator* allocator, FormatString<Args...> fmt, Args&&... args)
{
  auto text = ::std::format(fmt, ::std::forward<Args>(args)...);
  return detail::StringFromStd(detail::ResolveFormatAllocator(allocator), text);
}

template <typename... Args>
[[nodiscard]] String Format(FormatString<Args...> fmt, Args&&... args)
{
  return Format(nullptr, fmt, ::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] String Format(IAllocator& allocator, FormatString<Args...> fmt, Args&&... args)
{
  return Format(&allocator, fmt, ::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] String Format(IAllocator* allocator, RuntimeFormatString fmt, Args&&... args)
{
  auto text = ::std::vformat(detail::ToStdStringView(fmt.Value), ::std::make_format_args(args...));
  return detail::StringFromStd(detail::ResolveFormatAllocator(allocator), text);
}

template <typename... Args>
[[nodiscard]] String Format(RuntimeFormatString fmt, Args&&... args)
{
  return Format(nullptr, fmt, ::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] String Format(IAllocator& allocator, RuntimeFormatString fmt, Args&&... args)
{
  return Format(&allocator, fmt, ::std::forward<Args>(args)...);
}

template <typename... Args>
bool AppendFormat(String& out, FormatString<Args...> fmt, Args&&... args)
{
  auto text = ::std::format(fmt, ::std::forward<Args>(args)...);
  return out.Append({text.data(), text.size()});
}

template <typename... Args>
bool AppendFormat(String& out, RuntimeFormatString fmt, Args&&... args)
{
  auto text = ::std::vformat(detail::ToStdStringView(fmt.Value), ::std::make_format_args(args...));
  return out.Append({text.data(), text.size()});
}

}  // namespace gecko
