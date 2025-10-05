#pragma once

#include "hash.h"
#include "types.h"

namespace gecko {

struct Category {
  u32 Id{0};
  const char *Name{nullptr};
  constexpr explicit operator u32() const noexcept { return Id; }

  constexpr bool operator==(const Category &other) const noexcept {
    return Id == other.Id;
  }
  constexpr bool operator!=(const Category &other) const noexcept {
    return Id != other.Id;
  }
};

constexpr Category MakeCategory(const char *name) {
  return Category{FNV1a(name), name};
}
} // namespace gecko
