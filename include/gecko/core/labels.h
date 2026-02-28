#pragma once

#include "gecko/core/types.h"
#include "gecko/core/utility/hash.h"

namespace gecko {

struct Label
{
  u64 Id {0};
  const char* Name {nullptr};

  constexpr bool IsValid() const noexcept
  {
    return Id != 0;
  }

  constexpr bool operator==(const Label& other) const noexcept
  {
    return Id == other.Id;
  }
  constexpr bool operator!=(const Label& other) const noexcept
  {
    return Id != other.Id;
  }
};

constexpr Label MakeLabel(const char* fullName) noexcept
{
  if (!fullName || *fullName == '\0')
  {
    return {};
  }
  return Label {FNV1a64(fullName), fullName};
}
}  // namespace gecko
