#pragma once

#include "gecko/core/types.h"

namespace gecko {

struct PlacementTag
{};

inline constexpr PlacementTag Placement;

}  // namespace gecko

inline void* operator new(gecko::usize, void* memory, gecko::PlacementTag) noexcept
{
  return memory;
}

inline void operator delete(void*, void*, gecko::PlacementTag) noexcept
{}
