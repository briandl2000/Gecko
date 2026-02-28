#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace gecko {

using u8 = ::std::uint8_t;
using u16 = ::std::uint16_t;
using u32 = ::std::uint32_t;
using u64 = ::std::uint64_t;

using i8 = ::std::int8_t;
using i16 = ::std::int16_t;
using i32 = ::std::int32_t;
using i64 = ::std::int64_t;

using f32 = float;
using f64 = double;

using usize = ::std::size_t;
using isize = ::std::ptrdiff_t;

using byte = ::std::byte;

static_assert(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

static_assert(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
static_assert(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
static_assert(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
static_assert(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

static_assert(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
static_assert(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

static_assert(::std::numeric_limits<unsigned char>::digits == 8,
              "Expected 8-bit bytes.");
static_assert(::std::numeric_limits<f32>::is_iec559,
              "Expected IEEE-754 float.");
static_assert(::std::numeric_limits<f64>::is_iec559,
              "Expected IEEE-754 double.");

}  // namespace gecko
