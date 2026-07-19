#include "gecko/core/services/memory.h"

#include "gecko/core/placement.h"
#include "gecko/core/sync.h"

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
  SpinMutex Mutex;
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

  if (g_Arena.Last != nullptr && g_Arena.Last->Free)
  {
    // The committed range is contiguous with the free tail, so no new list
    // node is needed. In particular, Last->Next must remain null: a node
    // constructed at `start` would become interior storage after this merge.
    g_Arena.Last->Size += commitSize;
    g_Arena.Committed += commitSize;
    return true;
  }

  auto* block = new (start, Placement) Block {};
  block->Size = commitSize;
  block->Previous = g_Arena.Last;
  if (g_Arena.Last != nullptr)
  {
    g_Arena.Last->Next = block;
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

[[noreturn]] void InvalidMemoryOperation(const char* expression, const char* message, u32 line) noexcept
{
  AssertFailure(AssertInfo {
      .Expression = expression,
      .Message = message,
      .File = __FILE__,
      .Function = __func__,
      .Line = line,
  });
}

}  // namespace

void* AllocBytes(u64 requestedSize, u32 requestedAlignment) noexcept
{
  if (requestedSize == 0)
    InvalidMemoryOperation("requestedSize != 0", "Cannot allocate zero bytes", __LINE__);
  if (requestedAlignment == 0 || (requestedAlignment & (requestedAlignment - 1U)) != 0)
    InvalidMemoryOperation("requestedAlignment is a power of two", "Invalid allocation alignment", __LINE__);

  const usize size = static_cast<usize>(requestedSize);
  const usize alignment =
      requestedAlignment > alignof(AllocationHeader) ? requestedAlignment : alignof(AllocationHeader);

  LockGuard lock(g_Arena.Mutex);
  if (!ReserveArena())
    OutOfMemory(requestedSize);

  usize usedSize = 0;
  usize userOffset = 0;
  Block* block = FindBlock(size, alignment, usedSize, userOffset);
  if (block == nullptr)
  {
    const usize minimumCommit = sizeof(Block) + sizeof(AllocationHeader) + alignment - 1U + size;
    if (!CommitArena(minimumCommit))
      OutOfMemory(requestedSize);
    block = FindBlock(size, alignment, usedSize, userOffset);
  }

  if (block == nullptr)
    OutOfMemory(requestedSize);
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
  return user;
}

void DeallocBytes(void* memory) noexcept
{
  if (memory == nullptr)
    return;

  auto* header = reinterpret_cast<AllocationHeader*>(static_cast<u8*>(memory) - sizeof(AllocationHeader));
  if (header->Magic != AllocationMagic)
    InvalidMemoryOperation("header->Magic == AllocationMagic", "Pointer was not allocated by Gecko", __LINE__);

  LockGuard lock(g_Arena.Mutex);
  Block* block = header->Owner;
  if (block == nullptr || block->Free)
    InvalidMemoryOperation("block != nullptr && !block->Free", "Double free or corrupt allocation header", __LINE__);
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
}

MemoryStats GetMemoryStats() noexcept
{
  LockGuard lock(g_Arena.Mutex);
  const MemoryStats stats {
      .ReservedBytes = g_Arena.Reserved,
      .CommittedBytes = g_Arena.Committed,
      .LiveBytes = g_Arena.LiveBytes,
      .PeakBytes = g_Arena.PeakBytes,
      .AllocationCount = g_Arena.AllocationCount,
  };
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
