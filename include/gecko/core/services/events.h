#pragma once

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/types.h"

#include <cstddef>

namespace gecko {

struct Label;

// EventCode is 64-bit: upper 32 bits from module's Label.Id hash,
// lower 32 bits for module-local event code.
using EventCode = u64;

// Create a globally-unique event code from a module's Label ID and local code.
constexpr inline EventCode MakeEventCode(u64 moduleId, u32 localCode)
{
  return ((moduleId >> 32) << 32) | static_cast<u64>(localCode);
}

// Extract the module hash portion (upper 32 bits of EventCode).
constexpr inline u32 GetEventModule(EventCode code)
{
  return static_cast<u32>(code >> 32);
}

// Extract the module-local event code (lower 32 bits).
constexpr inline u32 GetEventLocal(EventCode code)
{
  return static_cast<u32>(code & 0xFFFFFFFFu);
}

// Legacy compatibility helpers (deprecated - use MakeEventCode instead)
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
  union
  {
    const void* ptr;
    u8 inlineData[16];
  };
  u32 size = 0;
  bool isInline = false;

  EventView() : ptr(nullptr)
  {}

  // Construct from pointer
  EventView(const void* p, u32 s) : ptr(p), size(s), isInline(false)
  {}

  // Get data pointer (works for both inline and pointer)
  const void* Data() const
  {
    return isInline ? inlineData : ptr;
  }
};

struct EventMeta
{
  EventCode code {};  // Packed: module hash (upper 32) + local code (lower 32)
  u64 moduleId {0};   // Full Label.Id of the module that sent this event
  u64 sender {0};     // Optional sender identifier
  u64 seq {0};        // Sequence number
};

struct EventEmitter
{
  u64 moduleId {0};    // Full Label.Id of the owning module
  u64 sender {0};      // Optional sender identifier
  u64 capability {0};  // Secret capability for validation
};

enum class SubscriptionDelivery : u8
{
  Queued = 0,    // Delivered when DispatchQueuedEvents() is called.
  OnPublish = 1  // Delivered immediately when events are published (e.g., via
                 // PublishEvent(), PublishImmediate(), or Enqueue()).
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

  // Register a module ID with the event bus (called during module
  // registration). Returns true if registered successfully, false if already
  // registered.
  GECKO_API virtual bool RegisterModule(u64 moduleId) = 0;

  // Unregister a module ID from the event bus.
  GECKO_API virtual void UnregisterModule(u64 moduleId) = 0;

  // Create an emitter for a registered module.
  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId,
                                               u64 sender = 0) = 0;

  // Validate that an emitter is authentic and matches the expected module.
  GECKO_API virtual bool ValidateEmitter(const EventEmitter& emitter,
                                         u64 expectedModuleId) const = 0;

protected:
  friend class EventSubscription;
  GECKO_API virtual void Unsubscribe(u64 id) = 0;
};

// Forward declaration for inline helper functions below
IEventBus* GetEventBus() noexcept;

// Create emitter for a registered module using its Label ID.
// This is the preferred way for modules to create emitters.
[[nodiscard]] GECKO_API inline EventEmitter CreateEmitter(u64 moduleId,
                                                          u64 sender = 0)
{
  if (auto* eventBus = GetEventBus())
    return eventBus->CreateEmitter(moduleId, sender);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

// Create emitter for a module using its Label.
// Looks up the module and uses its Label.Id.
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

// Delayed publish (preferred).
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

// Immediate publish (use sparingly).
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

// NullEventBus: No-op event bus (discards all events)
// All events and subscriptions are silently discarded
// Use runtime::EventBus for working event system
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
