#pragma once

#include "gecko/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/types.h"

namespace gecko {

struct Label;
using EventCode = u64;

constexpr EventCode MakeEventCode(u64 moduleId, u32 localCode) noexcept
{
  return ((moduleId >> 32U) << 32U) | static_cast<u64>(localCode);
}
constexpr u32 GetEventModule(EventCode code) noexcept
{
  return static_cast<u32>(code >> 32U);
}
constexpr u32 GetEventLocal(EventCode code) noexcept
{
  return static_cast<u32>(code & 0xFFFF'FFFFU);
}
constexpr EventCode MakeEvent(u8 domain, u32 localCode) noexcept
{
  return (static_cast<u64>(domain) << 24U) | (localCode & 0x00FF'FFFFU);
}
constexpr u8 EventDomain(EventCode code) noexcept
{
  return static_cast<u8>((code >> 24U) & 0xFFU);
}
constexpr u32 EventLocal(EventCode code) noexcept
{
  return static_cast<u32>(code & 0x00FF'FFFFU);
}

struct EventView
{
  const void* Bytes {nullptr};
  u32 Size {0};

  [[nodiscard]] const void* Data() const noexcept
  {
    return Bytes;
  }
};

struct EventMeta
{
  EventCode Code {0};
  u64 ModuleId {0};
  u64 Sender {0};
  u64 Sequence {0};
};

struct EventEmitter
{
  u64 ModuleId {0};
  u64 Sender {0};
  u64 Capability {0};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Capability != 0;
  }
};

enum class SubscriptionDelivery : u8
{
  Queued,
  Immediate,
};

struct SubscriptionOptions
{
  SubscriptionDelivery Delivery {SubscriptionDelivery::Queued};
};

using EventCallbackFn = void (*)(void* user, const EventMeta& meta, EventView payload);

class EventSubscription
{
public:
  EventSubscription() = default;
  explicit EventSubscription(u64 id) noexcept : m_Id(id)
  {}
  EventSubscription(const EventSubscription&) = delete;
  EventSubscription& operator=(const EventSubscription&) = delete;
  EventSubscription(EventSubscription&& other) noexcept : m_Id(other.m_Id)
  {
    other.m_Id = 0;
  }
  EventSubscription& operator=(EventSubscription&& other) noexcept
  {
    if (this != &other)
    {
      Reset();
      m_Id = other.m_Id;
      other.m_Id = 0;
    }
    return *this;
  }
  ~EventSubscription() noexcept
  {
    Reset();
  }

  GECKO_API void Reset() noexcept;
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Id != 0;
  }

private:
  u64 m_Id {0};
};

[[nodiscard]] GECKO_API bool RegisterEventModule(u64 moduleId) noexcept;
GECKO_API void UnregisterEventModule(u64 moduleId) noexcept;
[[nodiscard]] GECKO_API EventEmitter CreateEmitter(u64 moduleId, u64 sender = 0) noexcept;
[[nodiscard]] GECKO_API EventEmitter CreateEmitterForModule(Label moduleLabel, u64 sender = 0) noexcept;
[[nodiscard]] GECKO_API bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) noexcept;
[[nodiscard]] GECKO_API EventSubscription SubscribeEvent(EventCode code, EventCallbackFn callback, void* user,
                                                         SubscriptionOptions options = {}) noexcept;
GECKO_API void SendEvent(const EventEmitter& emitter, EventCode code, EventView payload) noexcept;
[[nodiscard]] GECKO_API usize DispatchEvents(usize maxCount = USizeMax) noexcept;

template <class T>
void SendEvent(const EventEmitter& emitter, EventCode code, const T& payload) noexcept
{
  SendEvent(emitter, code, EventView {&payload, static_cast<u32>(sizeof(T))});
}

}  // namespace gecko
