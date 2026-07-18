#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"
#include "gecko/core/version.h"

#include <stddef.h>

namespace gecko {

inline constexpr u32 GameApiVersion = 1;
inline constexpr const char* GameApiSymbol = "GeckoGame_GetApi";

struct GameContext
{
  u32 StructSize {sizeof(GameContext)};
  u32 EngineVersion {0};
  u32 EngineAbi {0};
  bool Reloaded {false};
};

struct GameFrame
{
  f64 DeltaSeconds {0.0};
  u64 FrameIndex {0};
};

struct GameApi
{
  u32 StructSize {sizeof(GameApi)};
  u32 ApiVersion {GameApiVersion};
  u32 BuiltWithEngineVersion {VersionPacked()};
  u32 BuiltWithEngineAbi {EngineAbiVersion};
  const char* BuiltWithEngineRelease {VersionFullString()};
  const char* Name {nullptr};

  bool (*Initialize)(const GameContext& context) noexcept {nullptr};
  bool (*Update)(const GameFrame& frame) noexcept {nullptr};
  void (*Shutdown)() noexcept {nullptr};
};

inline constexpr usize GameApiV1Size = offsetof(GameApi, Shutdown) + sizeof(((GameApi*)nullptr)->Shutdown);

using GetGameApiFn = const GameApi* (*)() noexcept;

}  // namespace gecko

#if defined(GECKO_PLATFORM_WINDOWS)
#define GECKO_GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define GECKO_GAME_EXPORT extern "C" __attribute__((visibility("default")))
#endif
