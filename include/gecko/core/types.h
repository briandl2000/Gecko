#pragma once

namespace gecko {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8 = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using f32 = float;
using f64 = double;

using usize = decltype(sizeof(0));
#if defined(_WIN64)
using isize = signed long long;
#elif defined(_WIN32)
using isize = signed int;
#else
using isize = __PTRDIFF_TYPE__;
#endif

enum class byte : u8
{
};

inline constexpr usize USizeMax = static_cast<usize>(-1);
inline constexpr u32 U32Max = static_cast<u32>(-1);
inline constexpr u64 U64Max = static_cast<u64>(-1);
inline constexpr i32 I32Max = static_cast<i32>(U32Max >> 1U);
inline constexpr i32 I32Min = -I32Max - 1;
inline constexpr i64 I64Max = static_cast<i64>(U64Max >> 1U);
inline constexpr i64 I64Min = -I64Max - 1;

static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);
static_assert(sizeof(i8) == 1);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(i64) == 8);
static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

}  // namespace gecko
