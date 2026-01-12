#pragma once

#include "gecko/core/labels.h"

namespace gecko::platform::labels {

inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
inline constexpr ::gecko::Label Window =
    ::gecko::MakeLabel("gecko.platform.window");
inline constexpr ::gecko::Label Input =
    ::gecko::MakeLabel("gecko.platform.input");
inline constexpr ::gecko::Label General =
    ::gecko::MakeLabel("gecko.platform.general");

} // namespace gecko::platform::labels
