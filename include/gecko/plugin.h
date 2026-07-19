#pragma once

#include "gecko/api.h"
#include "gecko/core/types.h"

#include <stddef.h>

namespace gecko {

inline constexpr u32 PluginApiVersion = 1;
inline constexpr const char* PluginApiSymbol = "GeckoPlugin_GetApi";

struct PluginContext
{
  u32 StructSize {sizeof(PluginContext)};
  bool Reloaded {false};
};

struct PluginFrame
{
  f64 DeltaSeconds {0.0};
  u64 FrameIndex {0};
};

struct PluginApi
{
  u32 StructSize {sizeof(PluginApi)};
  u32 ApiVersion {PluginApiVersion};
  const char* Name {nullptr};

  bool (*Initialize)(const PluginContext& context) noexcept {nullptr};
  bool (*Update)(const PluginFrame& frame) noexcept {nullptr};
  void (*Shutdown)() noexcept {nullptr};
};

inline constexpr usize PluginApiV1Size = offsetof(PluginApi, Shutdown) + sizeof(((PluginApi*)nullptr)->Shutdown);

using GetPluginApiFn = const PluginApi* (*)() noexcept;

}  // namespace gecko

#if defined(GECKO_PLATFORM_WINDOWS)
#define GECKO_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define GECKO_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif
