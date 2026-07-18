#include "gecko/core/services/memory.h"

#include "gecko/core/placement.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <sys/mman.h>
#endif

namespace gecko {

namespace {

constexpr usize ArenaReserveSize = 1ULL << 30U;
constexpr usize ArenaCommitSize = 64ULL << 20U;
constexpr u64 AllocationMagic = 0x4745434B4F4D454DULL;
constexpr u32 MaxLabelDepth = 32;

struct Block
{
  usize Size {0};
  Block* Previous {nullptr};
  Block* Next {nullptr};
  bool Free {true};
};

struct AllocationHeader
{
  u64 Magic {AllocationMagic};
  Block* Owner {nullptr};
  u64 RequestedSize {0};
  Label AllocationLabel {};
};

struct ArenaState
{
  u8* Base {nullptr};
  usize Reserved {0};
  usize Committed {0};
  Block* First {nullptr};
  Block* Last {nullptr};
  u32 Lock {0};
  u64 LiveBytes {0};
  u64 PeakBytes {0};
  u64 AllocationCount {0};
};

ArenaState g_Arena;
thread_local Label g_LabelStack[MaxLabelDepth] {};
thread_local u32 g_LabelDepth = 0;

[[nodiscard]] constexpr usize AlignUp(usize value, usize alignment) noexcept
{
  return (value + alignment - 1U) & ~(alignment - 1U);
}

void LockArena() noexcept
{
#if defined(_MSC_VER)
  while (_InterlockedExchange(reinterpret_cast<volatile long*>(&g_Arena.Lock), 1) != 0)
  {}
#else
  while (__atomic_exchange_n(&g_Arena.Lock, 1U, __ATOMIC_ACQUIRE) != 0)
  {}
#endif
}

void UnlockArena() noexcept
{
#if defined(_MSC_VER)
  (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&g_Arena.Lock), 0);
#else
  __atomic_store_n(&g_Arena.Lock, 0U, __ATOMIC_RELEASE);
#endif
}

[[nodiscard]] bool ReserveArena() noexcept
{
  if (g_Arena.Base != nullptr)
    return true;

#if defined(GECKO_PLATFORM_WINDOWS)
  g_Arena.Base = static_cast<u8*>(::VirtualAlloc(nullptr, ArenaReserveSize, MEM_RESERVE, PAGE_NOACCESS));
#elif defined(GECKO_PLATFORM_LINUX)
  void* memory = ::mmap(nullptr, ArenaReserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (memory != MAP_FAILED)
    g_Arena.Base = static_cast<u8*>(memory);
#endif
  if (g_Arena.Base == nullptr)
    return false;
  g_Arena.Reserved = ArenaReserveSize;
  return true;
}

[[nodiscard]] bool CommitArena(usize minimumSize) noexcept
{
  const usize commitSize = AlignUp(minimumSize > ArenaCommitSize ? minimumSize : ArenaCommitSize, ArenaCommitSize);
  if (g_Arena.Committed + commitSize > g_Arena.Reserved)
    return false;

  u8* start = g_Arena.Base + g_Arena.Committed;
#if defined(GECKO_PLATFORM_WINDOWS)
  if (::VirtualAlloc(start, commitSize, MEM_COMMIT, PAGE_READWRITE) == nullptr)
    return false;
#elif defined(GECKO_PLATFORM_LINUX)
  if (::mprotect(start, commitSize, PROT_READ | PROT_WRITE) != 0)
    return false;
#endif

  auto* block = new (start, Placement) Block {};
  block->Size = commitSize;
  block->Previous = g_Arena.Last;
  if (g_Arena.Last != nullptr)
  {
    g_Arena.Last->Next = block;
    if (g_Arena.Last->Free)
    {
      g_Arena.Last->Size += block->Size;
      g_Arena.Committed += commitSize;
      return true;
    }
  }
  else
  {
    g_Arena.First = block;
  }
  g_Arena.Last = block;
  g_Arena.Committed += commitSize;
  return true;
}

[[nodiscard]] Block* FindBlock(usize size, usize alignment, usize& usedSize, usize& userOffset) noexcept
{
  for (Block* block = g_Arena.First; block != nullptr; block = block->Next)
  {
    if (!block->Free)
      continue;

    const usize blockAddress = reinterpret_cast<usize>(block);
    const usize userAddress = AlignUp(blockAddress + sizeof(Block) + sizeof(AllocationHeader), alignment);
    userOffset = userAddress - blockAddress;
    usedSize = AlignUp(userOffset + size, alignof(Block));
    if (usedSize <= block->Size)
      return block;
  }
  return nullptr;
}

void SplitBlock(Block& block, usize usedSize) noexcept
{
  constexpr usize MinimumSplit = sizeof(Block) + 64;
  if (block.Size - usedSize < MinimumSplit)
    return;

  auto* remainder = new (reinterpret_cast<u8*>(&block) + usedSize, Placement) Block {};
  remainder->Size = block.Size - usedSize;
  remainder->Previous = &block;
  remainder->Next = block.Next;
  if (remainder->Next != nullptr)
    remainder->Next->Previous = remainder;
  else
    g_Arena.Last = remainder;
  block.Next = remainder;
  block.Size = usedSize;
}

void OutOfMemory(u64 size) noexcept
{
  AssertFailure(AssertInfo {
      .Expression = "Gecko arena has enough memory",
      .Message = "Out of memory",
      .File = __FILE__,
      .Function = __func__,
      .Line = __LINE__,
  });
  (void)size;
}

}  // namespace

void* AllocBytes(u64 requestedSize, u32 requestedAlignment) noexcept
{
  GECKO_ASSERT(requestedSize != 0, "Cannot allocate zero bytes");
  GECKO_ASSERT(requestedAlignment != 0 && (requestedAlignment & (requestedAlignment - 1U)) == 0,
               "Alignment must be a power of two");

  const usize size = static_cast<usize>(requestedSize);
  const usize alignment =
      requestedAlignment > alignof(AllocationHeader) ? requestedAlignment : alignof(AllocationHeader);

  LockArena();
  if (!ReserveArena())
  {
    UnlockArena();
    OutOfMemory(requestedSize);
  }

  usize usedSize = 0;
  usize userOffset = 0;
  Block* block = FindBlock(size, alignment, usedSize, userOffset);
  if (block == nullptr)
  {
    const usize minimumCommit = sizeof(Block) + sizeof(AllocationHeader) + alignment - 1U + size;
    if (!CommitArena(minimumCommit))
    {
      UnlockArena();
      OutOfMemory(requestedSize);
    }
    block = FindBlock(size, alignment, usedSize, userOffset);
  }

  GECKO_ASSERT(block != nullptr, "Committed memory did not produce a free block");
  SplitBlock(*block, usedSize);
  block->Free = false;

  u8* user = reinterpret_cast<u8*>(block) + userOffset;
  auto* header = reinterpret_cast<AllocationHeader*>(user - sizeof(AllocationHeader));
  *header = AllocationHeader {
      .Owner = block,
      .RequestedSize = requestedSize,
      .AllocationLabel = CurrentMemoryLabel(),
  };

  g_Arena.LiveBytes += requestedSize;
  if (g_Arena.LiveBytes > g_Arena.PeakBytes)
    g_Arena.PeakBytes = g_Arena.LiveBytes;
  ++g_Arena.AllocationCount;
  UnlockArena();
  return user;
}

void DeallocBytes(void* memory) noexcept
{
  if (memory == nullptr)
    return;

  auto* header = reinterpret_cast<AllocationHeader*>(static_cast<u8*>(memory) - sizeof(AllocationHeader));
  GECKO_ASSERT(header->Magic == AllocationMagic, "Pointer was not allocated by Gecko");

  LockArena();
  Block* block = header->Owner;
  GECKO_ASSERT(block != nullptr && !block->Free, "Double free or corrupt allocation header");
  g_Arena.LiveBytes -= header->RequestedSize;
  block->Free = true;
  header->Magic = 0;

  if (block->Next != nullptr && block->Next->Free)
  {
    Block* next = block->Next;
    block->Size += next->Size;
    block->Next = next->Next;
    if (block->Next != nullptr)
      block->Next->Previous = block;
    else
      g_Arena.Last = block;
  }
  if (block->Previous != nullptr && block->Previous->Free)
  {
    Block* previous = block->Previous;
    previous->Size += block->Size;
    previous->Next = block->Next;
    if (previous->Next != nullptr)
      previous->Next->Previous = previous;
    else
      g_Arena.Last = previous;
  }
  UnlockArena();
}

MemoryStats GetMemoryStats() noexcept
{
  LockArena();
  const MemoryStats stats {
      .ReservedBytes = g_Arena.Reserved,
      .CommittedBytes = g_Arena.Committed,
      .LiveBytes = g_Arena.LiveBytes,
      .PeakBytes = g_Arena.PeakBytes,
      .AllocationCount = g_Arena.AllocationCount,
  };
  UnlockArena();
  return stats;
}

void PushMemoryLabel(Label label) noexcept
{
  GECKO_ASSERT(g_LabelDepth < MaxLabelDepth, "Memory label stack overflow");
  g_LabelStack[g_LabelDepth++] = label;
}

void PopMemoryLabel() noexcept
{
  GECKO_ASSERT(g_LabelDepth != 0, "Memory label stack underflow");
  --g_LabelDepth;
}

Label CurrentMemoryLabel() noexcept
{
  return g_LabelDepth == 0 ? Label {} : g_LabelStack[g_LabelDepth - 1U];
}

}  // namespace gecko
