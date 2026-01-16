#pragma once

#include "gecko/core/labels.h"

namespace gecko::runtime::labels {

inline constexpr ::gecko::Label Runtime = ::gecko::MakeLabel("gecko.runtime");

inline constexpr ::gecko::Label General =
    ::gecko::MakeLabel("gecko.runtime.general");
inline constexpr ::gecko::Label JobSystem =
    ::gecko::MakeLabel("gecko.runtime.job_system");
inline constexpr ::gecko::Label TrackingAllocator =
    ::gecko::MakeLabel("gecko.runtime.tracking_allocator");
inline constexpr ::gecko::Label Logger =
    ::gecko::MakeLabel("gecko.runtime.logger");
inline constexpr ::gecko::Label Profiler =
    ::gecko::MakeLabel("gecko.runtime.profiler");
inline constexpr ::gecko::Label OperatorNew =
    ::gecko::MakeLabel("gecko.runtime.operator_new");
inline constexpr ::gecko::Label Modules =
    ::gecko::MakeLabel("gecko.runtime.modules");

}  // namespace gecko::runtime::labels
