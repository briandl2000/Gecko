#pragma once

/// @file
/// `EventBus` -- reference `IEventBus` implementation.
///
/// Supports both immediate and queued subscriber delivery, plus
/// per-module emitter capability validation.

#include "gecko/core/services/events.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gecko::runtime {

/// Reference event-bus implementation. Routes `Send()` calls to
/// matching subscribers either immediately on the caller's thread or
/// queued for `Dispatch()`.
class EventBus final : public IEventBus
{
public:
  EventBus();
  ~EventBus() override;

  EventSubscription Subscribe(EventCode code, CallbackFn fn, void* user,
                              SubscriptionOptions options = {}) noexcept override;
  void Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept override;
  usize Dispatch(usize maxCount) noexcept override;

  bool RegisterModule(u64 moduleId) noexcept override;
  void UnregisterModule(u64 moduleId) noexcept override;
  EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept override;
  bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

protected:
  void Unsubscribe(u64 id) noexcept override;

private:
  struct Subscriber
  {
    u64 id {0};
    CallbackFn callback {nullptr};
    void* user {nullptr};
    SubscriptionDelivery delivery {SubscriptionDelivery::Queued};
  };

  struct QueuedEvent
  {
    EventMeta meta {};
    u8 payloadStorage[256] {};
    u32 payloadSize {0};
  };

  void NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload, SubscriptionDelivery deliveryFilter);

  std::unique_ptr<std::unordered_map<EventCode, std::vector<Subscriber>>> m_Subscribers;
  std::mutex m_SubscribersMutex;
  std::unique_ptr<std::deque<QueuedEvent>> m_EventQueue;
  std::mutex m_QueueMutex;
  std::atomic<u64> m_NextSubscriptionId {1};
  std::atomic<u64> m_NextSequence {0};
  u64 m_CapabilitySecret {0};
  std::unique_ptr<std::unordered_set<u64>> m_RegisteredModules;
  std::mutex m_ModulesMutex;
};

}  // namespace gecko::runtime
