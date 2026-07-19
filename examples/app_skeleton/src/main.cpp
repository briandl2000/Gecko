#include "App.h"
#include "gecko/platform/terminal.h"

namespace {

using gecko::examples::app_skeleton::AppConfig;

void PrintUsage() noexcept
{
  using enum gecko::platform::TermStream;
  gecko::platform::PrintLine(Stderr, "usage: gecko_example_app_skeleton [options]");
  gecko::platform::PrintLine(Stderr, "  --no-window");
  gecko::platform::PrintLine(Stderr, "  --frames=N");
  gecko::platform::PrintLine(Stderr, "  --title=TEXT");
  gecko::platform::PrintLine(Stderr, "  --backend=auto|null|wayland|xlib");
}

bool ParseU32(gecko::StringView text, gecko::u32& output) noexcept
{
  if (text.Empty())
    return false;
  gecko::u64 value = 0;
  for (gecko::usize index = 0; index < text.Count(); ++index)
  {
    const char character = text[index];
    if (character < '0' || character > '9')
      return false;
    value = value * 10U + static_cast<gecko::u64>(character - '0');
    if (value > gecko::U32Max)
      return false;
  }
  output = static_cast<gecko::u32>(value);
  return true;
}

bool ParseArguments(int count, char** arguments, AppConfig& config) noexcept
{
  for (int index = 1; index < count; ++index)
  {
    const gecko::StringView argument {arguments[index]};
    if (argument == "--help" || argument == "-h")
    {
      PrintUsage();
      return false;
    }
    if (argument == "--no-window")
    {
      config.Windowed = false;
      continue;
    }
    if (argument.StartsWith("--frames="))
    {
      if (!ParseU32(argument.Substring(9), config.MaxFrames))
        return false;
      continue;
    }
    if (argument.StartsWith("--title="))
    {
      config.Title = arguments[index] + 8;
      continue;
    }
    if (argument.StartsWith("--backend="))
    {
      const gecko::StringView value = argument.Substring(10);
      if (value == "auto")
        config.Backend = gecko::platform::DisplayBackendKind::Auto;
      else if (value == "null")
        config.Backend = gecko::platform::DisplayBackendKind::Null;
      else if (value == "wayland")
        config.Backend = gecko::platform::DisplayBackendKind::Wayland;
      else if (value == "xlib")
        config.Backend = gecko::platform::DisplayBackendKind::Xlib;
      else
        return false;
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace

int main(int argumentCount, char** arguments)
{
  AppConfig config {};
  if (!ParseArguments(argumentCount, arguments, config))
    return 2;

  gecko::examples::app_skeleton::App app(config);
  return app.Run();
}
