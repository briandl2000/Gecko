#include "gecko/core/utility/random.h"

#include "gecko/core/assert.h"
#include "gecko/core/utility/time.h"
#include "gecko/platform/threading.h"

namespace gecko {

namespace {

thread_local u64 g_RandomState = 0;

u64 NextRandom() noexcept
{
  if (g_RandomState == 0)
    g_RandomState = MonotonicTimeNs() ^ (platform::CurrentThreadId() * 0x9E3779B97F4A7C15ULL);
  u64 value = g_RandomState;
  value ^= value >> 12U;
  value ^= value << 25U;
  value ^= value >> 27U;
  g_RandomState = value;
  return value * 0x2545F4914F6CDD1DULL;
}

u64 RandomRange(u64 range) noexcept
{
  if (range == 0)
    return NextRandom();
  const u64 threshold = (~range + 1ULL) % range;
  for (;;)
  {
    const u64 value = NextRandom();
    if (value >= threshold)
      return value % range;
  }
}

}  // namespace

u32 RandomU32(u32 minimum, u32 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  return minimum == maximum ? minimum : minimum + static_cast<u32>(RandomRange(static_cast<u64>(maximum) - minimum + 1U));
}

u64 RandomU64(u64 minimum, u64 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  return minimum == maximum ? minimum : minimum + RandomRange(maximum - minimum + 1U);
}

i32 RandomI32(i32 minimum, i32 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  const u64 range = static_cast<u64>(static_cast<i64>(maximum) - minimum) + 1U;
  return static_cast<i32>(static_cast<i64>(minimum) + static_cast<i64>(RandomRange(range)));
}

i64 RandomI64(i64 minimum, i64 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  const u64 range = static_cast<u64>(maximum) - static_cast<u64>(minimum) + 1U;
  return static_cast<i64>(static_cast<u64>(minimum) + RandomRange(range));
}

f32 RandomF32(f32 minimum, f32 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  const f32 normalized = static_cast<f32>(NextRandom() >> 40U) * (1.0F / 16777216.0F);
  return minimum + normalized * (maximum - minimum);
}

f64 RandomF64(f64 minimum, f64 maximum) noexcept
{
  GECKO_ASSERT(minimum <= maximum, "Random range minimum exceeds maximum");
  const f64 normalized = static_cast<f64>(NextRandom() >> 11U) * (1.0 / 9007199254740992.0);
  return minimum + normalized * (maximum - minimum);
}

bool RandomBool() noexcept
{
  return (NextRandom() & 1U) != 0;
}

void RandomBytes(void* buffer, usize size) noexcept
{
  GECKO_ASSERT(buffer != nullptr || size == 0, "Random byte buffer cannot be null");
  auto* output = static_cast<u8*>(buffer);
  while (size != 0)
  {
    u64 value = NextRandom();
    for (u32 index = 0; index < 8 && size != 0; ++index, --size)
    {
      *output++ = static_cast<u8>(value);
      value >>= 8U;
    }
  }
}

void SeedRandom(u64 seed) noexcept
{
  g_RandomState = seed != 0 ? seed : 0xA5A5A5A5A5A5A5A5ULL;
}

}  // namespace gecko
