#pragma once

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/types.h"

#include <cstddef>

namespace gecko {

struct Label;

using EventCode = u64;

constexpr inline EventCode MakeEventCode(u64 moduleId, u32 localCode)
{
  return ((moduleId >> 32) << 32) | static_cast<u64>(localCode);
}

constexpr inline u32 GetEventModule(EventCode code)
{
  return static_cast<u32>(code >> 32);
}

constexpr inline u32 GetEventLocal(EventCode code)
{
  return static_cast<u32>(code & 0xFFFFFFFFu);
}

constexpr inline EventCode MakeEvent(u8 domain, u32 local24)
{
  return (static_cast<u64>(domain) << 24) | (local24 & 0x00FF'FFFFu);
}

constexpr inline u8 EventDomain(EventCode code)
{
  return static_cast<u8>((code >> 24) & 0xFFu);
}

constexpr inline u32 EventLocal(EventCode code)
{
  return static_cast<u32>(code & 0x00FF'FFFFu);
}

struct EventView
{
  const void* ptr {nullptr};
  u32 size {0};

  EventView() = default;

  EventView(const void* p, u32 s) : ptr(p), size(s)
  {}

  const void* Data() const
  {
    return ptr;
  }
};

struct EventMeta
{
  EventCode code {};
  u64 moduleId {0};
  u64 sender {0};
  u64 seq {0};
};

struct EventEmitter
{
  u64 moduleId {0};
  u64 sender {0};
  u64 capability {0};
};

enum class SubscriptionDelivery : u8
{
  Queued = 0,
  OnPublish = 1
};

struct SubscriptionOptions
{
  SubscriptionDelivery delivery {SubscriptionDelivery::Queued};
};

class IEventBus;

class EventSubscription
{
public:
  EventSubscription() = default;
  EventSubscription(const EventSubscription&) = delete;
  EventSubscription& operator=(const EventSubscription&) = delete;

  EventSubscription(EventSubscription&& other) noexcept
      : m_Bus(other.m_Bus), m_Id(other.m_Id)
  {
    other.m_Bus = nullptr;
    other.m_Id = 0;
  }

  EventSubscription& operator=(EventSubscription&& other)
  {
    if (this != &other)
    {
      Reset();
      m_Bus = other.m_Bus;
      m_Id = other.m_Id;
      other.m_Bus = nullptr;
      other.m_Id = 0;
    }
    return *this;
  }

  ~EventSubscription()
  {
    Reset();
  }

  GECKO_API void Reset();

  void SetSubscriptionData(IEventBus* bus, u64 id)
  {
    m_Bus = bus;
    m_Id = id;
  }

private:
  IEventBus* m_Bus = nullptr;
  u64 m_Id = 0;
};

class IEventBus
{
public:
  using CallbackFn = void (*)(void* user, const EventMeta& meta,
                              EventView payload);

  GECKO_API virtual ~IEventBus() = default;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual EventSubscription Subscribe(
      EventCode code, CallbackFn fn, void* user,
      SubscriptionOptions options = {}) = 0;

  GECKO_API virtual void PublishImmediate(const EventEmitter& emitter,
                                          EventCode code,
                                          EventView payload) = 0;

  template <class T>
  void PublishImmediate(const EventEmitter& emitter, EventCode code,
                        const T& payload)
  {
    PublishImmediate(emitter, code,
                     EventView {&payload, static_cast<u32>(sizeof(T))});
  }

  GECKO_API virtual void Enqueue(const EventEmitter& emitter, EventCode code,
                                 EventView payload) = 0;

  template <class T>
  void Enqueue(const EventEmitter& emitter, EventCode code, const T& payload)
  {
    Enqueue(emitter, code, EventView {&payload, static_cast<u32>(sizeof(T))});
  }

  GECKO_API virtual std::size_t DispatchQueued(
      std::size_t maxCount = static_cast<std::size_t>(-1)) = 0;

  GECKO_API virtual bool RegisterModule(u64 moduleId) = 0;

  GECKO_API virtual void UnregisterModule(u64 moduleId) = 0;

  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId,
                                               u64 sender = 0) = 0;

  GECKO_API virtual bool ValidateEmitter(const EventEmitter& emitter,
                                         u64 expectedModuleId) const = 0;

protected:
  friend class EventSubscription;
  GECKO_API virtual void Unsubscribe(u64 id) = 0;
};

IEventBus* GetEventBus() noexcept;

[[nodiscard]] GECKO_API inline EventEmitter CreateEmitter(u64 moduleId,
                                                          u64 sender = 0)
{
  if (auto* eventBus = GetEventBus())
    return eventBus->CreateEmitter(moduleId, sender);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

[[nodiscard]] GECKO_API EventEmitter CreateEmitterForModule(Label moduleLabel,
                                                            u64 sender = 0);

[[nodiscard]]
GECKO_API inline EventSubscription SubscribeEvent(
    EventCode code, IEventBus::CallbackFn fn, void* user,
    SubscriptionOptions options = {})
{
  if (auto* eventBus = GetEventBus())
    return eventBus->Subscribe(code, fn, user, options);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

GECKO_API inline void PublishEvent(const EventEmitter& emitter, EventCode code,
                                   EventView payload)
{
  if (auto* eventBus = GetEventBus())
    eventBus->Enqueue(emitter, code, payload);
  else
    GECKO_ASSERT(false && "No event bus detected!");
}

template <class T>
inline void PublishEvent(const EventEmitter& emitter, EventCode code,
                         const T& payload)
{
  PublishEvent(emitter, code,
               EventView {&payload, static_cast<u32>(sizeof(T))});
}

inline void PublishImmediateEvent(const EventEmitter& emitter, EventCode code,
                                  EventView payload)
{
  if (auto* eventBus = GetEventBus())
    eventBus->PublishImmediate(emitter, code, payload);
  else
    GECKO_ASSERT(false && "No event bus detected!");
}

template <class T>
inline void PublishImmediateEvent(const EventEmitter& emitter, EventCode code,
                                  const T& payload)
{
  PublishImmediateEvent(emitter, code,
                        EventView {&payload, static_cast<u32>(sizeof(T))});
}

[[nodiscard]]
GECKO_API inline std::size_t DispatchQueuedEvents(
    std::size_t maxCount = static_cast<std::size_t>(-1))
{
  if (auto* eventBus = GetEventBus())
    return eventBus->DispatchQueued(maxCount);
  GECKO_ASSERT(false && "No event bus detected!");
  return 0;
}

struct NullEventBus final : IEventBus
{
  GECKO_API virtual EventSubscription Subscribe(
      EventCode code, CallbackFn fn, void* user,
      SubscriptionOptions options = {}) noexcept override;
  GECKO_API virtual void PublishImmediate(const EventEmitter& emitter,
                                          EventCode code,
                                          EventView payload) noexcept override;
  GECKO_API virtual void Enqueue(const EventEmitter& emitter, EventCode code,
                                 EventView payload) noexcept override;
  GECKO_API virtual std::size_t DispatchQueued(
      std::size_t maxCount) noexcept override;

  GECKO_API virtual bool RegisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual void UnregisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId,
                                               u64 sender) noexcept override;
  GECKO_API virtual bool ValidateEmitter(
      const EventEmitter& emitter,
      u64 expectedModuleId) const noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;

protected:
  GECKO_API virtual void Unsubscribe(u64 id) noexcept override;
};

}  // namespace gecko
