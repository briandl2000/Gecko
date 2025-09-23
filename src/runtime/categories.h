#pragma once
#include "gecko/core/category.h"

namespace gecko::runtime::categories {
  inline constexpr auto General = MakeCategory("runtime");
  inline constexpr auto TrackingAllocator = MakeCategory("runtime::tracking_allocator");
  inline constexpr auto Logger = MakeCategory("runtime::Logger");
  inline constexpr auto OperatorNew = MakeCategory("operator_new");
}
