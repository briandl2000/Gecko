#pragma once

#include "gecko/core/labels.h"

namespace gecko::graphics::labels {

inline constexpr ::gecko::Label Graphics = ::gecko::MakeLabel("gecko.graphics");
inline constexpr ::gecko::Label General =
    ::gecko::MakeLabel("gecko.graphics.general");
inline constexpr ::gecko::Label Vulkan =
    ::gecko::MakeLabel("gecko.graphics.vulkan");

}  // namespace gecko::graphics::labels
