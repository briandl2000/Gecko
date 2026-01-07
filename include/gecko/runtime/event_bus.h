#pragma once

#include "gecko/core/events.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace gecko::runtime {

class EventBus final : public IEventBus {
public:
  EventBus();
  ~EventBus() override;

  EventSubscription Subscribe(EventCode code, CallbackFn fn,
                              void *user,
                              SubscriptionOptions options = {}) noexcept override;
  void PublishImmediate(const EventEmitter &emitter, EventCode code,
                        EventView payload) noexcept override;
  void Enqueue(const EventEmitter &emitter, EventCode code,
               EventView payload) noexcept override;
  std::size_t DispatchQueued(std::size_t maxCount) noexcept override;
  EventEmitter CreateEmitter(u8 domain, u64 sender) noexcept override;
  bool ValidateEmitter(const EventEmitter &emitter,
                       u8 expectedDomain) const noexcept override;
  bool Init() noexcept override;
  void Shutdown() noexcept override;

protected:
  void Unsubscribe(u64 id) noexcept override;

private:
  struct Subscriber {
    u64 id{0};
    CallbackFn callback{nullptr};
    void *user{nullptr};
    SubscriptionDelivery delivery{SubscriptionDelivery::Queued};
  };

  struct QueuedEvent {
    EventMeta meta{};
    u8 payloadStorage[256]{};
    u32 payloadSize{0};
  };

  void PublishToSubscribers(EventCode code, const EventMeta &meta,
                            EventView payload);
  void PublishToSubscribers(EventCode code, const EventMeta &meta,
                            EventView payload,
                            SubscriptionDelivery deliveryFilter);

  std::unordered_map<EventCode, std::vector<Subscriber>> m_Subscribers;
  std::vector<QueuedEvent> m_EventQueue;
  std::mutex m_QueueMutex;
  std::atomic<u64> m_NextSubscriptionId{1};
  std::atomic<u64> m_NextSequence{0};
  u64 m_CapabilitySecret{0};
};

} // namespace gecko::runtime
