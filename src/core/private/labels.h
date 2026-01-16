#pragma once

#include "gecko/core/labels.h"

namespace gecko::core::labels {

inline constexpr ::gecko::Label Core = ::gecko::MakeLabel("gecko.core");

inline constexpr ::gecko::Label General =
    ::gecko::MakeLabel("gecko.core.general");
inline constexpr ::gecko::Label Services =
    ::gecko::MakeLabel("gecko.core.services");
inline constexpr ::gecko::Label Events =
    ::gecko::MakeLabel("gecko.core.events");
inline constexpr ::gecko::Label Modules =
    ::gecko::MakeLabel("gecko.core.modules");
inline constexpr ::gecko::Label Random =
    ::gecko::MakeLabel("gecko.core.random");
inline constexpr ::gecko::Label Thread =
    ::gecko::MakeLabel("gecko.core.thread");
inline constexpr ::gecko::Label Time = ::gecko::MakeLabel("gecko.core.time");

}  // namespace gecko::core::labels
