#pragma once

/// @file
/// Event bus service: typed pub/sub messaging between modules.
///
/// Events carry a `EventCode` (a packed module/local id), a typed
/// payload viewed through `EventView`, and metadata. Subscribers
/// receive events either inline at `Send` time (`Immediate`) or when
/// the dispatcher pumps the queue (`Queued`).

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/types.h"

#include <cstddef>

namespace gecko {

struct Label;

/// Globally unique event identifier. Pack with `MakeEventCode` /
/// `MakeEvent`.
using EventCode = u64;

/// Pack `moduleId` and a 32-bit `localCode` into a single event code.
constexpr inline EventCode MakeEventCode(u64 moduleId, u32 localCode)
{
  return ((moduleId >> 32) << 32) | static_cast<u64>(localCode);
}

/// @return The 32 high bits (module id) of `code`.
constexpr inline u32 GetEventModule(EventCode code)
{
  return static_cast<u32>(code >> 32);
}

/// @return The 32 low bits (local code) of `code`.
constexpr inline u32 GetEventLocal(EventCode code)
{
  return static_cast<u32>(code & 0xFFFFFFFFu);
}

/// Compact pack of an 8-bit `domain` and a 24-bit `local24` code.
constexpr inline EventCode MakeEvent(u8 domain, u32 local24)
{
  return (static_cast<u64>(domain) << 24) | (local24 & 0x00FF'FFFFu);
}

/// @return The 8-bit domain field of a `MakeEvent`-style code.
constexpr inline u8 EventDomain(EventCode code)
{
  return static_cast<u8>((code >> 24) & 0xFFu);
}

/// @return The 24-bit local field of a `MakeEvent`-style code.
constexpr inline u32 EventLocal(EventCode code)
{
  return static_cast<u32>(code & 0x00FF'FFFFu);
}

/// Non-owning view over an event payload buffer. Lifetime is tied to
/// the originating `Send` call (queued events are copied internally).
struct EventView
{
  const void* ptr {nullptr};  ///< Start of payload.
  u32 size {0};               ///< Bytes available at `ptr`.

  EventView() = default;

  EventView(const void* p, u32 s) : ptr(p), size(s)
  {}

  /// @return The payload pointer.
  const void* Data() const
  {
    return ptr;
  }
};

/// Per-event metadata delivered to subscribers alongside the payload.
struct EventMeta
{
  EventCode code {};  ///< The event code as passed to `Send`.
  u64 moduleId {0};   ///< Originating module id.
  u64 sender {0};     ///< Optional caller-supplied sender id.
  u64 seq {0};        ///< Monotonically increasing sequence number.
};

/// Capability token proving a module is allowed to publish events.
/// Obtain via `IEventBus::CreateEmitter` / `CreateEmitterForModule`.
struct EventEmitter
{
  u64 moduleId {0};    ///< Module that owns this emitter.
  u64 sender {0};      ///< Caller-supplied sender id (forwarded to subscribers).
  u64 capability {0};  ///< Opaque token validated by the bus on `Send`.
};

/// Whether a subscriber is invoked synchronously or batched on the
/// dispatcher.
enum class SubscriptionDelivery : u8
{
  Queued = 0,    ///< Invoked from `Dispatch`.
  Immediate = 1  ///< Invoked inline from `Send`.
};

/// Per-subscription configuration.
struct SubscriptionOptions
{
  SubscriptionDelivery delivery {SubscriptionDelivery::Queued};
};

class IEventBus;

/// RAII handle owning an active subscription. Destruction or `Reset`
/// unsubscribes; copies are disabled to prevent double-unsubscribe.
class EventSubscription
{
public:
  EventSubscription() = default;
  EventSubscription(const EventSubscription&) = delete;
  EventSubscription& operator=(const EventSubscription&) = delete;

  EventSubscription(EventSubscription&& other) noexcept : m_Bus(other.m_Bus), m_Id(other.m_Id)
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

  /// Drop the subscription if any. Idempotent.
  GECKO_API void Reset();

  /// Internal: bind a fresh handle to a bus/id pair.
  void SetSubscriptionData(IEventBus* bus, u64 id)
  {
    m_Bus = bus;
    m_Id = id;
  }

private:
  IEventBus* m_Bus = nullptr;
  u64 m_Id = 0;
};

/// Event bus service interface.
class IEventBus
{
public:
  /// Subscriber callback signature.
  using CallbackFn = void (*)(void* user, const EventMeta& meta, EventView payload);

  GECKO_API virtual ~IEventBus() = default;

  /// One-time setup; called when the bus is installed.
  GECKO_API virtual bool Init() noexcept = 0;
  /// Counterpart to `Init`; pending events are dropped.
  GECKO_API virtual void Shutdown() noexcept = 0;

  /// Register a callback for an event code.
  ///
  /// @param code     Event code to subscribe to.
  /// @param fn       Callback invoked when matching events are delivered.
  /// @param user     Opaque pointer forwarded to `fn`.
  /// @param options  Delivery options (queued vs. immediate).
  /// @return RAII handle that unsubscribes on destruction.
  [[nodiscard]]
  GECKO_API virtual EventSubscription Subscribe(EventCode code, CallbackFn fn, void* user,
                                                SubscriptionOptions options = {}) = 0;

  /// Publish an event. Immediate subscribers run inline; queued
  /// subscribers run on the next `Dispatch`.
  ///
  /// @param emitter  Capability minted via `CreateEmitter`.
  /// @param code     Event code.
  /// @param payload  Untyped view over the payload bytes.
  GECKO_API virtual void Send(const EventEmitter& emitter, EventCode code, EventView payload) = 0;

  /// Convenience overload that views `payload` as raw bytes.
  template <class T>
  void Send(const EventEmitter& emitter, EventCode code, const T& payload)
  {
    Send(emitter, code, EventView {&payload, static_cast<u32>(sizeof(T))});
  }

  /// Drain queued events, invoking subscribers on the calling thread.
  ///
  /// @param maxCount  Maximum number of events to dispatch in this call.
  /// @return Number of events actually dispatched.
  GECKO_API virtual usize Dispatch(usize maxCount = static_cast<usize>(-1)) = 0;

  /// Register a module so it may create emitters.
  /// @param moduleId  Stable id (e.g. `Label::Hash`) of the module.
  GECKO_API virtual bool RegisterModule(u64 moduleId) = 0;

  /// Counterpart to `RegisterModule`; cancels in-flight emitters.
  /// @param moduleId  Module previously passed to `RegisterModule`.
  GECKO_API virtual void UnregisterModule(u64 moduleId) = 0;

  /// Mint an `EventEmitter` capability.
  ///
  /// @param moduleId  Owning module id.
  /// @param sender    Optional caller-supplied sender id, forwarded
  ///                  verbatim to subscribers.
  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId, u64 sender = 0) = 0;

  /// Validate an emitter capability.
  ///
  /// @param emitter            Token produced by `CreateEmitter`.
  /// @param expectedModuleId   Module id the caller expects to own it.
  /// @return `true` if `emitter` is a valid capability for
  ///         `expectedModuleId`.
  GECKO_API virtual bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const = 0;

protected:
  friend class EventSubscription;
  GECKO_API virtual void Unsubscribe(u64 id) = 0;
};

/// @return Active event bus, or `NullEventBus` if none is installed.
IEventBus* GetEventBus() noexcept;

/// Convenience wrapper around `IEventBus::CreateEmitter`.
[[nodiscard]] GECKO_API inline EventEmitter CreateEmitter(u64 moduleId, u64 sender = 0)
{
  if (auto* eventBus = GetEventBus())
    return eventBus->CreateEmitter(moduleId, sender);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

/// Look up a module by its `Label` and create an emitter for it.
[[nodiscard]] GECKO_API EventEmitter CreateEmitterForModule(Label moduleLabel, u64 sender = 0);

/// Convenience wrapper around `IEventBus::Subscribe`.
[[nodiscard]]
GECKO_API inline EventSubscription SubscribeEvent(EventCode code, IEventBus::CallbackFn fn, void* user,
                                                  SubscriptionOptions options = {})
{
  if (auto* eventBus = GetEventBus())
    return eventBus->Subscribe(code, fn, user, options);
  GECKO_ASSERT(false && "No event bus detected!");
  return {};
}

/// Convenience wrapper around `IEventBus::Send`.
GECKO_API inline void SendEvent(const EventEmitter& emitter, EventCode code, EventView payload)
{
  if (auto* eventBus = GetEventBus())
    eventBus->Send(emitter, code, payload);
  else
    GECKO_ASSERT(false && "No event bus detected!");
}

/// Convenience overload that views `payload` as raw bytes.
template <class T>
inline void SendEvent(const EventEmitter& emitter, EventCode code, const T& payload)
{
  SendEvent(emitter, code, EventView {&payload, static_cast<u32>(sizeof(T))});
}

/// Drive the bus's queue. @return Number of events dispatched.
[[nodiscard]]
GECKO_API inline usize DispatchEvents(usize maxCount = static_cast<usize>(-1))
{
  if (auto* eventBus = GetEventBus())
    return eventBus->Dispatch(maxCount);
  GECKO_ASSERT(false && "No event bus detected!");
  return 0;
}

struct NullEventBus final : IEventBus
{
  GECKO_API virtual EventSubscription Subscribe(EventCode code, CallbackFn fn, void* user,
                                                SubscriptionOptions options = {}) noexcept override;
  GECKO_API virtual void Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept override;
  GECKO_API virtual usize Dispatch(usize maxCount) noexcept override;

  GECKO_API virtual bool RegisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual void UnregisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept override;
  GECKO_API virtual bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;

protected:
  GECKO_API virtual void Unsubscribe(u64 id) noexcept override;
};

}  // namespace gecko
