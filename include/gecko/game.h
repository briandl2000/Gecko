#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

inline constexpr u32 GameApiVersion = 1;
inline constexpr const char* GameApiSymbol = "GeckoGame_GetApi";

struct GameContext
{
  u32 StructSize {sizeof(GameContext)};
  u32 EngineVersion {0};
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
  const char* Name {nullptr};

  bool (*Initialize)(const GameContext& context) noexcept {nullptr};
  bool (*Update)(const GameFrame& frame) noexcept {nullptr};
  void (*Shutdown)() noexcept {nullptr};
};

using GetGameApiFn = const GameApi* (*)() noexcept;

}  // namespace gecko

#if defined(GECKO_PLATFORM_WINDOWS)
#define GECKO_GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define GECKO_GAME_EXPORT extern "C" __attribute__((visibility("default")))
#endif
