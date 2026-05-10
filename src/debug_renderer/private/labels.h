#pragma once

#include "gecko/core/labels.h"
#include "gecko/debug_renderer/debug_renderer_module.h"

namespace gecko::debug_renderer::labels {

// labels::DebugRenderer is defined in debug_renderer_module.h (public API).

inline constexpr ::gecko::Label Pipeline = ::gecko::MakeLabel("gecko.debug_renderer.pipeline");
inline constexpr ::gecko::Label Context = ::gecko::MakeLabel("gecko.debug_renderer.context");

}  // namespace gecko::debug_renderer::labels
