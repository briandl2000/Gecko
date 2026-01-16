#pragma once

#include "gecko/core/events.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gecko::runtime {

class EventBus final : public IEventBus
{
public:
  EventBus();
  ~EventBus() override;

  EventSubscription Subscribe(
      EventCode code, CallbackFn fn, void* user,
      SubscriptionOptions options = {}) noexcept override;
  void PublishImmediate(const EventEmitter& emitter, EventCode code,
                        EventView payload) noexcept override;
  void Enqueue(const EventEmitter& emitter, EventCode code,
               EventView payload) noexcept override;
  std::size_t DispatchQueued(std::size_t maxCount) noexcept override;

  bool RegisterModule(u64 moduleId) noexcept override;
  void UnregisterModule(u64 moduleId) noexcept override;
  EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept override;
  bool ValidateEmitter(const EventEmitter& emitter,
                       u64 expectedModuleId) const noexcept override;

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

  void PublishToSubscribers(EventCode code, const EventMeta& meta,
                            EventView payload);
  void PublishToSubscribers(EventCode code, const EventMeta& meta,
                            EventView payload,
                            SubscriptionDelivery deliveryFilter);

  std::unordered_map<EventCode, std::vector<Subscriber>> m_Subscribers;
  std::mutex m_SubscribersMutex;
  std::deque<QueuedEvent> m_EventQueue;
  std::mutex m_QueueMutex;
  std::atomic<u64> m_NextSubscriptionId {1};
  std::atomic<u64> m_NextSequence {0};
  u64 m_CapabilitySecret {0};
  std::unordered_set<u64> m_RegisteredModules;
  std::mutex m_ModulesMutex;
};

}  // namespace gecko::runtime
