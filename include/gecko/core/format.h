#pragma once

#include "gecko/api.h"
#include "gecko/core/span.h"
#include "gecko/core/containers/string.h"
#include "gecko/core/types.h"

namespace gecko {

enum class FormatArgKind : u8
{
  Signed,
  Unsigned,
  Float,
  String,
  StringView,
  Pointer,
  Character,
  Boolean,
};

struct FormatArg
{
  struct ViewValue
  {
    const char* Data;
    usize Count;
  };

  FormatArgKind Kind {FormatArgKind::String};
  union
  {
    i64 Signed;
    u64 Unsigned;
    f64 Float;
    const char* String;
    ViewValue View;
    const void* Pointer;
    char Character;
    bool Boolean;
  } Value {.String = nullptr};
};

struct FormatBuffer
{
  char* Data {nullptr};
  usize Capacity {0};
  usize Length {0};
  bool Truncated {false};
};

GECKO_API void FormatTo(FormatBuffer& output, const char* format, Span<const FormatArg> arguments = {}) noexcept;

template <typename T>
struct FormatInteger;

#define GECKO_FORMAT_INTEGER(type, is_signed) \
  template <>                                 \
  struct FormatInteger<type>                  \
  {                                           \
    static constexpr bool Signed = is_signed; \
  }

GECKO_FORMAT_INTEGER(signed char, true);
GECKO_FORMAT_INTEGER(unsigned char, false);
GECKO_FORMAT_INTEGER(signed short, true);
GECKO_FORMAT_INTEGER(unsigned short, false);
GECKO_FORMAT_INTEGER(signed int, true);
GECKO_FORMAT_INTEGER(unsigned int, false);
GECKO_FORMAT_INTEGER(signed long, true);
GECKO_FORMAT_INTEGER(unsigned long, false);
GECKO_FORMAT_INTEGER(signed long long, true);
GECKO_FORMAT_INTEGER(unsigned long long, false);

#undef GECKO_FORMAT_INTEGER

template <typename T>
concept FormattableInteger = requires { FormatInteger<T>::Signed; };

template <FormattableInteger T>
[[nodiscard]] constexpr FormatArg MakeFormatArg(T value) noexcept
{
  if constexpr (FormatInteger<T>::Signed)
    return FormatArg {.Kind = FormatArgKind::Signed, .Value = {.Signed = static_cast<i64>(value)}};
  else
    return FormatArg {.Kind = FormatArgKind::Unsigned, .Value = {.Unsigned = static_cast<u64>(value)}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(bool value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Boolean, .Value = {.Boolean = value}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(char value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Character, .Value = {.Character = value}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(float value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Float, .Value = {.Float = static_cast<f64>(value)}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(double value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Float, .Value = {.Float = value}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(long double value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Float, .Value = {.Float = static_cast<f64>(value)}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(const char* value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::String, .Value = {.String = value}};
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(StringView value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::StringView,
                    .Value = {.View = {.Data = value.Data(), .Count = value.Count()}}};
}

[[nodiscard]] inline FormatArg MakeFormatArg(const String& value) noexcept
{
  return MakeFormatArg(value.View());
}

template <usize Capacity>
[[nodiscard]] FormatArg MakeFormatArg(const StaticString<Capacity>& value) noexcept
{
  return MakeFormatArg(value.View());
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(char* value) noexcept
{
  return MakeFormatArg(static_cast<const char*>(value));
}

template <usize Count>
[[nodiscard]] constexpr FormatArg MakeFormatArg(const char (&value)[Count]) noexcept
{
  return MakeFormatArg(static_cast<const char*>(value));
}

[[nodiscard]] constexpr FormatArg MakeFormatArg(const void* value) noexcept
{
  return FormatArg {.Kind = FormatArgKind::Pointer, .Value = {.Pointer = value}};
}

template <typename T>
[[nodiscard]] constexpr FormatArg MakeFormatArg(T* value) noexcept
{
  return MakeFormatArg(static_cast<const void*>(value));
}

template <typename T>
  requires __is_enum
(T) [[nodiscard]] constexpr FormatArg MakeFormatArg(T value) noexcept
{
  return MakeFormatArg(static_cast<__underlying_type(T)>(value));
}

template <typename First, typename... Rest>
void FormatTo(FormatBuffer& output, const char* format, const First& first, const Rest&... rest) noexcept
{
  const FormatArg arguments[] {MakeFormatArg(first), MakeFormatArg(rest)...};
  FormatTo(output, format, Span<const FormatArg> {arguments, 1U + sizeof...(rest)});
}

namespace detail {

template <typename First, typename... Rest>
[[noreturn]] void DispatchAssert(const char* expression, const char* file, u32 line, const char* function,
                                 const char* format, const First& first, const Rest&... rest) noexcept
{
  char message[1024] {};
  FormatBuffer output {.Data = message, .Capacity = sizeof(message)};
  FormatTo(output, format, first, rest...);
  AssertFailure(AssertInfo {
      .Expression = expression,
      .Message = message,
      .File = file,
      .Function = function,
      .Line = line,
  });
}

}  // namespace detail

}  // namespace gecko
