#pragma once

#include "gecko/core/labels.h"
#include "gecko/platform/platform_module.h"

namespace gecko::platform::labels {

// labels::Platform is defined in platform_module.h (public API)

inline constexpr ::gecko::Label Window =
    ::gecko::MakeLabel("gecko.platform.window");
inline constexpr ::gecko::Label Input =
    ::gecko::MakeLabel("gecko.platform.input");
inline constexpr ::gecko::Label General =
    ::gecko::MakeLabel("gecko.platform.general");

}  // namespace gecko::platform::labels
