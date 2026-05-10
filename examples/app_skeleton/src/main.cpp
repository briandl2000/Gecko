#include "App.h"

#include <cstdio>
#include <cstring>
#include <string_view>

namespace {

using ::gecko::examples::app_skeleton::AppConfig;

void PrintUsage(const char* exe)
{
  ::std::fprintf(stderr,
                 "Usage: %s [options]\n\n"
                 "Options:\n"
                 "  --no-window           Run without creating a window\n"
                 "  --frames=N            Run N frames then exit (windowed only)\n"
                 "  --title=TEXT          Window title (windowed only)\n"
                 "  --backend=auto|null|xlib  Window backend (Linux only supports "
                 "xlib/auto today)\n"
                 "  --help                Show this help\n",
                 exe ? exe : "app_skeleton");
}

bool StartsWith(::std::string_view s, ::std::string_view prefix)
{
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool ParseU32(::std::string_view s, ::gecko::u32& out)
{
  if (s.empty())
    return false;
  ::gecko::u64 value = 0;
  for (char c : s)
  {
    if (c < '0' || c > '9')
      return false;
    value = value * 10 + static_cast<::gecko::u64>(c - '0');
    if (value > 0xFFFFFFFFu)
      return false;
  }
  out = static_cast<::gecko::u32>(value);
  return true;
}

bool ParseArgs(int argc, char** argv, AppConfig& cfg)
{
  for (int i = 1; i < argc; ++i)
  {
    ::std::string_view arg = argv[i] ? argv[i] : "";

    if (arg == "--help" || arg == "-h")
    {
      PrintUsage(argv[0]);
      return false;
    }
    if (arg == "--no-window")
    {
      cfg.windowed = false;
      continue;
    }
    if (StartsWith(arg, "--frames="))
    {
      auto value = arg.substr(::std::strlen("--frames="));
      if (!ParseU32(value, cfg.maxFrames))
      {
        ::std::fprintf(stderr, "Invalid --frames value: %.*s\n", static_cast<int>(value.size()), value.data());
        return false;
      }
      continue;
    }
    if (StartsWith(arg, "--title="))
    {
      cfg.title = argv[i] + ::std::strlen("--title=");
      continue;
    }
    if (StartsWith(arg, "--backend="))
    {
      using ::gecko::platform::DisplayBackendKind;
      auto value = arg.substr(::std::strlen("--backend="));
      if (value == "auto")
        cfg.backend = DisplayBackendKind::Auto;
      else if (value == "null")
        cfg.backend = DisplayBackendKind::Null;
      else if (value == "xlib")
        cfg.backend = DisplayBackendKind::Xlib;
      else
      {
        ::std::fprintf(stderr, "Invalid --backend value: %.*s\n", static_cast<int>(value.size()), value.data());
        return false;
      }
      continue;
    }

    ::std::fprintf(stderr, "Unknown arg: %.*s\n", static_cast<int>(arg.size()), arg.data());
    PrintUsage(argv[0]);
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv)
{
  AppConfig cfg {};
  if (!ParseArgs(argc, argv, cfg))
    return 2;

  ::gecko::examples::app_skeleton::App app(cfg);
  return app.Run();
}
