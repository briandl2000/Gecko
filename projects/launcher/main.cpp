#include "gecko/core/services/log.h"
#include "gecko/core/utility/time.h"
#include "gecko/core/version.h"
#include "gecko/game.h"
#include "gecko/gecko.h"
#include "gecko/platform/shared_library.h"

namespace {

constexpr gecko::Label LauncherLabel = gecko::MakeLabel("gecko.launcher");

#if defined(GECKO_PLATFORM_WINDOWS)
constexpr const char* GameLibraryPath = "gecko_game.dll";
#else
constexpr const char* GameLibraryPath = "./libgecko_game.so";
#endif

struct LoadedGame
{
  gecko::platform::SharedLibrary Library;
  const gecko::GameApi* Api {nullptr};
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

bool LoadGame(LoadedGame& game) noexcept
{
  game.Library = gecko::platform::LoadSharedLibrary(GameLibraryPath);
  if (!game.Library.IsValid())
  {
    GECKO_ERROR(LauncherLabel, "Could not load %s: %s", GameLibraryPath, gecko::platform::SharedLibraryError());
    return false;
  }

  const auto getApi =
      gecko::platform::FindSharedLibraryFunction<gecko::GetGameApiFn>(game.Library, gecko::GameApiSymbol);
  if (getApi == nullptr)
  {
    GECKO_ERROR(LauncherLabel, "%s does not export %s", GameLibraryPath, gecko::GameApiSymbol);
    gecko::platform::UnloadSharedLibrary(game.Library);
    game = {};
    return false;
  }

  game.Api = getApi();
  if (game.Api == nullptr || game.Api->StructSize < sizeof(gecko::GameApi) ||
      game.Api->ApiVersion != gecko::GameApiVersion || game.Api->Initialize == nullptr || game.Api->Update == nullptr ||
      game.Api->Shutdown == nullptr)
  {
    GECKO_ERROR(LauncherLabel, "Game API is missing or incompatible");
    gecko::platform::UnloadSharedLibrary(game.Library);
    game = {};
    return false;
  }

  return true;
}

void UnloadGame(LoadedGame& game) noexcept
{
  if (game.Api != nullptr)
    game.Api->Shutdown();
  gecko::platform::UnloadSharedLibrary(game.Library);
  game = {};
}

}  // namespace

int main(int argumentCount, char** arguments)
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Sandbox";
  config.GraphicsBackend = gecko::graphics::GraphicsBackend::Vulkan;
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
    else if (!ParseFrameCount(argument, maxFrames))
      return 64;
  }
#if defined(_DEBUG)
  config.EnableGraphicsDebug = true;
#endif

  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  GECKO_INFO(LauncherLabel, "Gecko %s", gecko::VersionFullString());

  LoadedGame game {};
  if (!LoadGame(game))
  {
    gecko::Shutdown();
    return 2;
  }

  gecko::GameContext context {};
  context.EngineVersion = (static_cast<gecko::u32>(gecko::VersionMajor()) << 24U) |
                          (static_cast<gecko::u32>(gecko::VersionMinor()) << 16U) |
                          static_cast<gecko::u32>(gecko::VersionPatch());
  if (!game.Api->Initialize(context))
  {
    UnloadGame(game);
    gecko::Shutdown();
    return 3;
  }

  GECKO_INFO(LauncherLabel, "Running game: %s", game.Api->Name != nullptr ? game.Api->Name : "unnamed");

  gecko::u64 previousTime = gecko::MonotonicTimeNs();
  gecko::u64 frameIndex = 0;
  bool running = true;
  while (running)
  {
    const gecko::u64 now = gecko::MonotonicTimeNs();
    const gecko::GameFrame frame {
        .DeltaSeconds = gecko::time::NsToSecondsF(now - previousTime),
        .FrameIndex = frameIndex++,
    };
    previousTime = now;
    running = game.Api->Update(frame);
    if (maxFrames != 0 && frameIndex >= maxFrames)
      running = false;
  }

  UnloadGame(game);
  gecko::Shutdown();
  return 0;
}
