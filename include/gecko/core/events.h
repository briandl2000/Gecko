#pragma once

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "types.h"
#include <cstddef>
#include <utility>

namespace gecko {

using EventCode = u32;

constexpr inline EventCode MakeEvent(u8 domain, u32 local24) {
  return (static_cast<u32>(domain) << 24) | (local24 & 0x00FF'FFFFu);
}

constexpr inline u8 EventDomain(EventCode code) {
  return static_cast<u8>((code >> 24) & 0xFFu);
}

constexpr inline u32 EventLocal(EventCode code) {
  return static_cast<u32>(code & 0x00FF'FFFFu);
}

namespace core_events {
constexpr u8 kCoreDomain = 0x00;
} // namespace core_events

struct EventView {
  const void *data = nullptr;
  u32 size = 0;
};

struct EventMeta {
  EventCode code{};
  u64 sender{0};
  u64 seq{0};
};

struct EventEmitter {
  u8 domain{0};
  u64 sender{0};
  u64 capability{0};
};

class IEventBus;

class EventSubscription {
public:
  EventSubscription() = default;
  EventSubscription(const EventSubscription &) = delete;
  EventSubscription &operator=(const EventSubscription &) = delete;

  EventSubscription(EventSubscription &&other) noexcept
      : m_Bus(other.m_Bus), m_Id(other.m_Id) {
    other.m_Bus = nullptr;
    other.m_Id = 0;
  }

  EventSubscription &operator=(EventSubscription &&other) {
    if (this != &other) {
      Reset();
      m_Bus = other.m_Bus;
      m_Id = other.m_Id;
      other.m_Bus = nullptr;
      other.m_Id = 0;
    }
    return *this;
  }

  ~EventSubscription() { Reset(); }

  GECKO_API void Reset();

private:
  friend class IEventBus;
  IEventBus *m_Bus = nullptr;
  u64 m_Id = 0;
};

class IEventBus {
public:
  using CallbackFn = void (*)(void *user, const EventMeta &meta,
                              EventView payload);

  GECKO_API virtual ~IEventBus() = default;

  [[nodiscard]]
  GECKO_API virtual EventSubscription Subscribe(EventCode code, CallbackFn fn,
                                                void *user) = 0;

  GECKO_API virtual void PublishImmediate(const EventEmitter &emitter,
                                          EventCode code,
                                          EventView payload) = 0;

  template <class T>
  void PublishImmediate(const EventEmitter &emitter, EventCode code,
                        const T &payload) {
    PublishImmediate(emitter, code,
                     EventView{&payload, static_cast<u32>(sizeof(T))});
  }

  GECKO_API virtual void Enqueue(const EventEmitter &emitter, EventCode code,
                                 EventView payload) = 0;

  template <class T>
  void Enqueue(const EventEmitter &emitter, EventCode code, const T &payload) {
    Enqueue(emitter, code, EventView{&payload, static_cast<u32>(sizeof(T))});
  }

  GECKO_API virtual std::size_t
  DispatchQueued(std::size_t maxCount = static_cast<std::size_t>(-1)) = 0;

  GECKO_API virtual EventEmitter CreateEmitter(u8 domain, u64 sender = 0) = 0;

  GECKO_API virtual bool ValidateEmitter(const EventEmitter &emitter,
                                         u8 expectedDomain) const = 0;

protected:
  friend class EventSubscription;
  GECKO_API virtual void Unsubscribe(u64 id) = 0;
};

IEventBus *GetEventBus();
void InstallEventBusService(IEventBus *bus);

[[nodiscard]]
GECKO_API inline EventSubscription
SubscribeEvent(EventCode code, IEventBus::CallbackFn fn, void *user) {
  if (auto *eventBus = GetEventBus())
    return eventBus->Subscribe(code, fn, user);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

// Delayed publish (preferred).
GECKO_API inline void PublishEvent(const EventEmitter &emitter, EventCode code,
                                   EventView payload) {
  if (auto *eventBus = GetEventBus())
    eventBus->Enqueue(emitter, code, payload);
  else
    GECKO_ASSERT(false && "No event bus detected!");
}

template <class T>
inline void PublishEvent(const EventEmitter &emitter, EventCode code,
                         const T &payload) {
  PublishEvent(emitter, code, EventView{&payload, static_cast<u32>(sizeof(T))});
}

// Immediate publish (use sparingly).
inline void PublishImmediateEvent(const EventEmitter &emitter, EventCode code,
                                  EventView payload) {
  if (auto *eventBus = GetEventBus())
    eventBus->PublishImmediate(emitter, code, payload);
  else
    GECKO_ASSERT(false && "No event bus detected!");
}

template <class T>
inline void PublishImmediateEvent(const EventEmitter &emitter, EventCode code,
                                  const T &payload) {
  PublishImmediateEvent(emitter, code,
                        EventView{&payload, static_cast<u32>(sizeof(T))});
}

[[nodiscard]]
GECKO_API inline std::size_t
DispatchQueuedEvents(std::size_t maxCount = static_cast<std::size_t>(-1)) {
  if (auto *eventBus = GetEventBus())
    return eventBus->DispatchQueued(maxCount);
  GECKO_ASSERT(false && "No event bus detected!");
  return 0;
}

} // namespace gecko
