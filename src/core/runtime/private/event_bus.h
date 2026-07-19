#pragma once

#include "gecko/core/services/events.h"
#include "gecko/core/sync.h"

namespace gecko::runtime {

class EventBus final
{
public:
  EventSubscription Subscribe(EventCode code, EventCallbackFn fn, void* user,
                              SubscriptionOptions options = {}) noexcept;
  void Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept;
  usize Dispatch(usize maxCount) noexcept;

  bool RegisterModule(u64 moduleId) noexcept;
  void UnregisterModule(u64 moduleId) noexcept;
  EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept;
  bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept;

  bool Init() noexcept;
  void Shutdown() noexcept;

  void Unsubscribe(u64 id) noexcept;

private:
  static constexpr u32 MaxSubscribers = 1024;
  static constexpr u32 MaxQueuedEvents = 1024;
  static constexpr u32 MaxRegisteredModules = 64;
  static constexpr u32 MaxPayloadSize = 256;

  struct Subscriber
  {
    EventCode Code {0};
    u64 Id {0};
    EventCallbackFn Callback {nullptr};
    void* User {nullptr};
    SubscriptionDelivery Delivery {SubscriptionDelivery::Queued};
    bool Active {false};
  };

  struct QueuedEvent
  {
    EventMeta Meta {};
    u8 Payload[MaxPayloadSize] {};
    u32 PayloadSize {0};
  };

  void NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload,
                         SubscriptionDelivery delivery) noexcept;
  Subscriber m_Subscribers[MaxSubscribers] {};
  QueuedEvent m_Queue[MaxQueuedEvents] {};
  u64 m_Modules[MaxRegisteredModules] {};
  mutable Mutex m_Mutex;
  u32 m_QueueRead {0};
  u32 m_QueueCount {0};
  u32 m_ModuleCount {0};
  u64 m_NextSubscriptionId {1};
  u64 m_NextSequence {0};
  u64 m_CapabilitySecret {0};
  bool m_Initialized {false};
};

}  // namespace gecko::runtime
