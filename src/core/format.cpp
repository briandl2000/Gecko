#include "gecko/core/format.h"

namespace gecko {

namespace {

struct FormatSpec
{
  u32 Width {0};
  u32 Precision {3};
  char Fill {' '};
  char Align {0};
  char Type {0};
  bool HasPrecision {false};
  bool Alternate {false};
};

void AppendCharacter(FormatBuffer& output, char value) noexcept
{
  if (output.Data != nullptr && output.Capacity != 0 && output.Length + 1U < output.Capacity)
    output.Data[output.Length] = value;
  else
    output.Truncated = true;
  ++output.Length;
}

void Finish(FormatBuffer& output) noexcept
{
  if (output.Data == nullptr || output.Capacity == 0)
    return;
  const usize terminator = output.Length < output.Capacity ? output.Length : output.Capacity - 1U;
  output.Data[terminator] = '\0';
}

usize TextLength(const char* text) noexcept
{
  if (text == nullptr)
    return 6;
  usize length = 0;
  while (text[length] != '\0')
    ++length;
  return length;
}

void AppendText(FormatBuffer& output, const char* text, usize length) noexcept
{
  if (text == nullptr)
  {
    text = "(null)";
    length = 6;
  }
  for (usize index = 0; index < length; ++index)
    AppendCharacter(output, text[index]);
}

void AppendPadding(FormatBuffer& output, usize count, char fill = ' ') noexcept
{
  for (usize index = 0; index < count; ++index)
    AppendCharacter(output, fill);
}

usize UnsignedText(char* text, u64 value, u32 base, bool uppercase) noexcept
{
  char reverse[64];
  usize count = 0;
  do
  {
    const u32 digit = static_cast<u32>(value % base);
    reverse[count++] = static_cast<char>(digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
    value /= base;
  } while (value != 0);

  for (usize index = 0; index < count; ++index)
    text[index] = reverse[count - index - 1U];
  return count;
}

FormatSpec ParseSpec(const char*& cursor, const char* end) noexcept
{
  FormatSpec spec;
  if (cursor == end || *cursor != ':')
    return spec;
  ++cursor;

  if (cursor + 1 < end && (cursor[1] == '<' || cursor[1] == '>'))
  {
    spec.Fill = cursor[0];
    spec.Align = cursor[1];
    cursor += 2;
  }
  else if (cursor < end && (*cursor == '<' || *cursor == '>'))
  {
    spec.Align = *cursor++;
  }
  if (cursor < end && *cursor == '#')
  {
    spec.Alternate = true;
    ++cursor;
  }
  if (cursor < end && *cursor == '0')
    spec.Fill = '0';
  while (cursor < end && *cursor >= '0' && *cursor <= '9')
  {
    spec.Width = spec.Width * 10U + static_cast<u32>(*cursor - '0');
    ++cursor;
  }
  if (cursor < end && *cursor == '.')
  {
    ++cursor;
    spec.HasPrecision = true;
    spec.Precision = 0;
    while (cursor < end && *cursor >= '0' && *cursor <= '9')
    {
      spec.Precision = spec.Precision * 10U + static_cast<u32>(*cursor - '0');
      ++cursor;
    }
    if (spec.Precision > 9)
      spec.Precision = 9;
  }
  if (cursor < end)
    spec.Type = *cursor++;
  return spec;
}

void AppendField(FormatBuffer& output, const char* text, usize length, const FormatSpec& spec, bool numeric) noexcept
{
  const usize padding = spec.Width > length ? spec.Width - length : 0;
  const bool left = spec.Align == '<' || (spec.Align == 0 && !numeric);
  if (!left)
  {
    if (numeric && spec.Fill == '0' && length != 0 && text[0] == '-')
    {
      AppendCharacter(output, '-');
      AppendPadding(output, padding, '0');
      AppendText(output, text + 1, length - 1U);
      return;
    }
    if (numeric && spec.Fill == '0' && length > 2U && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
      AppendText(output, text, 2);
      AppendPadding(output, padding, '0');
      AppendText(output, text + 2, length - 2U);
      return;
    }
    AppendPadding(output, padding, numeric ? spec.Fill : ' ');
  }
  AppendText(output, text, length);
  if (left)
    AppendPadding(output, padding, spec.Fill);
}

void AppendArgument(FormatBuffer& output, const FormatArg& argument, const FormatSpec& spec) noexcept
{
  char text[96];
  usize length = 0;
  bool numeric = false;

  switch (argument.Kind)
  {
  case FormatArgKind::String: {
    const char* value = argument.Value.String;
    length = TextLength(value);
    if (spec.HasPrecision && length > spec.Precision)
      length = spec.Precision;
    AppendField(output, value, length, spec, false);
    return;
  }
  case FormatArgKind::StringView: {
    usize length = argument.Value.View.Count;
    if (spec.HasPrecision && length > spec.Precision)
      length = spec.Precision;
    AppendField(output, argument.Value.View.Data, length, spec, false);
    return;
  }
  case FormatArgKind::Character:
    text[length++] = argument.Value.Character;
    break;
  case FormatArgKind::Boolean: {
    const char* value = argument.Value.Boolean ? "true" : "false";
    AppendField(output, value, TextLength(value), spec, false);
    return;
  }
  case FormatArgKind::Pointer:
    text[length++] = '0';
    text[length++] = 'x';
    length += UnsignedText(text + length, reinterpret_cast<usize>(argument.Value.Pointer), 16, false);
    numeric = true;
    break;
  case FormatArgKind::Signed: {
    const i64 value = argument.Value.Signed;
    const bool negative = value < 0;
    const u64 magnitude = negative ? static_cast<u64>(-(value + 1)) + 1U : static_cast<u64>(value);
    if (negative)
      text[length++] = '-';
    const bool hex = spec.Type == 'x' || spec.Type == 'X';
    if (hex && spec.Alternate)
    {
      text[length++] = '0';
      text[length++] = spec.Type;
    }
    length += UnsignedText(text + length, magnitude, hex ? 16U : 10U, spec.Type == 'X');
    numeric = true;
    break;
  }
  case FormatArgKind::Unsigned: {
    const bool hex = spec.Type == 'x' || spec.Type == 'X';
    if (hex && spec.Alternate)
    {
      text[length++] = '0';
      text[length++] = spec.Type;
    }
    length += UnsignedText(text + length, argument.Value.Unsigned, hex ? 16U : 10U, spec.Type == 'X');
    numeric = true;
    break;
  }
  case FormatArgKind::Float: {
    f64 value = argument.Value.Float;
    if (value != value)
    {
      AppendField(output, "nan", 3, spec, true);
      return;
    }
    if (value < 0)
    {
      text[length++] = '-';
      value = -value;
    }
    const u32 precision = spec.HasPrecision ? spec.Precision : 3U;
    u64 scale = 1;
    for (u32 digit = 0; digit < precision; ++digit)
      scale *= 10U;
    if (value > static_cast<f64>(U64Max / scale))
    {
      AppendField(output, "<float>", 7, spec, true);
      return;
    }
    const u64 scaled = static_cast<u64>(value * static_cast<f64>(scale) + 0.5);
    length += UnsignedText(text + length, scaled / scale, 10, false);
    if (precision != 0)
    {
      text[length++] = '.';
      u64 fraction = scaled % scale;
      u64 divisor = scale / 10U;
      for (u32 digit = 0; digit < precision; ++digit)
      {
        text[length++] = static_cast<char>('0' + (fraction / divisor) % 10U);
        divisor = divisor > 1 ? divisor / 10U : 1U;
      }
    }
    numeric = true;
    break;
  }
  }

  AppendField(output, text, length, spec, numeric);
}

}  // namespace

void FormatTo(FormatBuffer& output, const char* format, Span<const FormatArg> arguments) noexcept
{
  if (format == nullptr)
  {
    AppendText(output, "(null format)", 13);
    Finish(output);
    return;
  }

  usize argumentIndex = 0;
  while (*format != '\0')
  {
    if (*format == '{' && format[1] == '{')
    {
      AppendCharacter(output, '{');
      format += 2;
      continue;
    }
    if (*format == '}' && format[1] == '}')
    {
      AppendCharacter(output, '}');
      format += 2;
      continue;
    }
    if (*format != '{')
    {
      AppendCharacter(output, *format++);
      continue;
    }

    const char* end = format + 1;
    while (*end != '\0' && *end != '}')
      ++end;
    if (*end == '\0')
    {
      AppendText(output, "<format-error>", 14);
      break;
    }

    const char* specCursor = format + 1;
    const FormatSpec spec = ParseSpec(specCursor, end);
    if (specCursor != end || argumentIndex >= arguments.Count())
      AppendText(output, "<format-error>", 14);
    else
      AppendArgument(output, arguments[argumentIndex++], spec);
    format = end + 1;
  }
  Finish(output);
}

}  // namespace gecko
