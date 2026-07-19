#include "gecko/gecko.h"

namespace {

constexpr gecko::Label LauncherLabel = gecko::MakeLabel("gecko.launcher");

#if defined(GECKO_PLATFORM_WINDOWS)
constexpr const char* PluginLibraryName = "gecko_sandbox.dll";
#else
constexpr const char* PluginLibraryName = "libgecko_sandbox.so";
#endif

struct LoadedPlugin
{
  gecko::platform::SharedLibrary Library;
  const gecko::PluginApi* Api {nullptr};
};

bool TextEquals(const char* a, const char* b) noexcept
{
  if (a == nullptr || b == nullptr)
    return false;
  while (*a != '\0' && *a == *b)
  {
    ++a;
    ++b;
  }
  return *a == *b;
}

bool ParseFrameCount(const char* argument, gecko::u64& frameCount) noexcept
{
  constexpr const char* Prefix = "--frames=";
  const char* cursor = argument;
  const char* prefix = Prefix;
  while (*prefix != '\0' && *cursor == *prefix)
  {
    ++cursor;
    ++prefix;
  }
  if (*prefix != '\0' || *cursor == '\0')
    return false;

  gecko::u64 result = 0;
  while (*cursor != '\0')
  {
    if (*cursor < '0' || *cursor > '9')
      return false;
    result = result * 10U + static_cast<gecko::u64>(*cursor - '0');
    ++cursor;
  }
  frameCount = result;
  return true;
}

gecko::String ResolvePluginLibraryPath() noexcept
{
  const gecko::String executable = gecko::platform::ExePath();
  const gecko::StringView path = executable.View();
  gecko::usize separator = path.FindLast('/');
  const gecko::usize backslash = path.FindLast('\\');
  if (separator == gecko::StringView::NotFound || (backslash != gecko::StringView::NotFound && backslash > separator))
    separator = backslash;

  gecko::String result;
  if (separator != gecko::StringView::NotFound)
    result.Assign(path.Substring(0, separator + 1U));
  result.Append(PluginLibraryName);
  return result;
}

bool LoadPlugin(LoadedPlugin& plugin) noexcept
{
  const gecko::String libraryPath = ResolvePluginLibraryPath();
  plugin.Library = gecko::platform::LoadSharedLibrary(libraryPath.CStr());
  if (!plugin.Library.IsValid())
  {
    GECKO_ERROR(LauncherLabel, "Could not load {}: {}", libraryPath, gecko::platform::SharedLibraryError());
    return false;
  }

  const auto getApi =
      gecko::platform::FindSharedLibraryFunction<gecko::GetPluginApiFn>(plugin.Library, gecko::PluginApiSymbol);
  if (getApi == nullptr)
  {
    GECKO_ERROR(LauncherLabel, "{} does not export {}", libraryPath, gecko::PluginApiSymbol);
    gecko::platform::UnloadSharedLibrary(plugin.Library);
    plugin = {};
    return false;
  }

  plugin.Api = getApi();
  if (plugin.Api == nullptr || plugin.Api->StructSize < gecko::PluginApiV1Size ||
      plugin.Api->ApiVersion != gecko::PluginApiVersion || plugin.Api->Initialize == nullptr ||
      plugin.Api->Update == nullptr || plugin.Api->Shutdown == nullptr)
  {
    GECKO_ERROR(LauncherLabel, "Plugin API is missing or incompatible");
    gecko::platform::UnloadSharedLibrary(plugin.Library);
    plugin = {};
    return false;
  }
  return true;
}

void UnloadPlugin(LoadedPlugin& plugin) noexcept
{
  if (plugin.Api != nullptr)
    plugin.Api->Shutdown();
  gecko::platform::UnloadSharedLibrary(plugin.Library);
  plugin = {};
}

}  // namespace

int main(int argumentCount, char** arguments)
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Sandbox";
  gecko::u64 maxFrames = 0;

  for (int index = 1; index < argumentCount; ++index)
  {
    const char* argument = arguments[index];
    if (TextEquals(argument, "--backend=wayland"))
      config.Platform.Backend = gecko::platform::DisplayBackendKind::Wayland;
    else if (TextEquals(argument, "--backend=x11"))
      config.Platform.Backend = gecko::platform::DisplayBackendKind::Xlib;
    else if (TextEquals(argument, "--backend=null"))
      config.Platform.Backend = gecko::platform::DisplayBackendKind::Null;
    else if (TextEquals(argument, "--graphics=null"))
      config.Graphics.Backend = gecko::graphics::GraphicsBackend::Null;
    else if (!ParseFrameCount(argument, maxFrames))
      return 64;
  }
#if defined(_DEBUG)
  config.Graphics.Debug = true;
#endif

  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  GECKO_INFO(LauncherLabel, "Gecko {}", gecko::VersionFullString());

  LoadedPlugin plugin {};
  if (!LoadPlugin(plugin))
  {
    gecko::Shutdown();
    return 2;
  }

  const gecko::PluginContext context {};
  if (!plugin.Api->Initialize(context))
  {
    UnloadPlugin(plugin);
    gecko::Shutdown();
    return 3;
  }

  GECKO_INFO(LauncherLabel, "Running plugin: {}", plugin.Api->Name != nullptr ? plugin.Api->Name : "unnamed");

  gecko::u64 previousTime = gecko::MonotonicTimeNs();
  gecko::u64 frameIndex = 0;
  bool running = true;
  while (running)
  {
    const gecko::u64 now = gecko::MonotonicTimeNs();
    const gecko::PluginFrame frame {
        .DeltaSeconds = gecko::time::NsToSecondsF(now - previousTime),
        .FrameIndex = frameIndex++,
    };
    previousTime = now;
    running = plugin.Api->Update(frame);
    if (maxFrames != 0 && frameIndex >= maxFrames)
      running = false;
  }

  UnloadPlugin(plugin);
  gecko::Shutdown();
  return 0;
}
