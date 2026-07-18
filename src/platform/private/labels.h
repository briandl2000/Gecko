#pragma once

#include "gecko/core/labels.h"

namespace gecko::platform::labels {

inline constexpr Label Platform = MakeLabel("gecko.platform");
inline constexpr Label Window = MakeLabel("gecko.platform.window");
inline constexpr Label Input = MakeLabel("gecko.platform.input");
inline constexpr Label General = MakeLabel("gecko.platform.general");

}  // namespace gecko::platform::labels
