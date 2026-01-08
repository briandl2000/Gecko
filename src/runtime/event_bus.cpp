#include "gecko/runtime/event_bus.h"
#include "gecko/core/assert.h"
#include "gecko/core/random.h"
#include <algorithm>
#include <cstring>

namespace gecko::runtime {

EventBus::EventBus() { m_CapabilitySecret = RandomU64(); }

EventBus::~EventBus() { m_Subscribers.clear(); }

bool EventBus::Init() noexcept { return true; }

void EventBus::Shutdown() noexcept {
  m_Subscribers.clear();
  std::lock_guard<std::mutex> lock(m_QueueMutex);
  m_EventQueue.clear();
}

EventSubscription EventBus::Subscribe(EventCode code, CallbackFn fn,
                                      void *user,
                                      SubscriptionOptions options) noexcept {
  GECKO_ASSERT(fn && "Callback cannot be null");

  u64 id = m_NextSubscriptionId.fetch_add(1, std::memory_order_relaxed);

  Subscriber sub{};
  sub.id = id;
  sub.callback = fn;
  sub.user = user;
  sub.delivery = options.delivery;

  m_Subscribers[code].push_back(sub);

  EventSubscription subscription{};
  subscription.SetSubscriptionData(this, id);
  return subscription;
}

void EventBus::Unsubscribe(u64 id) noexcept {
  if (id == 0)
    return;

  for (auto &[code, subscribers] : m_Subscribers) {
    auto it = std::find_if(subscribers.begin(), subscribers.end(),
                           [id](const Subscriber &s) { return s.id == id; });
    if (it != subscribers.end()) {
      subscribers.erase(it);
      return;
    }
  }
}

void EventBus::PublishImmediate(const EventEmitter &emitter, EventCode code,
                                EventView payload) noexcept {
  GECKO_ASSERT(ValidateEmitter(emitter, EventDomain(code)) &&
               "Invalid emitter for event domain");

  EventMeta meta{};
  meta.code = code;
  meta.sender = emitter.sender;
  meta.seq = m_NextSequence.fetch_add(1, std::memory_order_relaxed);

  PublishToSubscribers(code, meta, payload);
}

void EventBus::Enqueue(const EventEmitter &emitter, EventCode code,
                       EventView payload) noexcept {
  GECKO_ASSERT(ValidateEmitter(emitter, EventDomain(code)) &&
               "Invalid emitter for event domain");
  GECKO_ASSERT(payload.size <= sizeof(QueuedEvent::payloadStorage) &&
               "Payload too large for queue");

  QueuedEvent qEvent{};
  qEvent.meta.code = code;
  qEvent.meta.sender = emitter.sender;
  qEvent.meta.seq = m_NextSequence.fetch_add(1, std::memory_order_relaxed);
  qEvent.payloadSize = payload.size;

  const void *data = payload.Data();
  if (data && payload.size > 0) {
    std::memcpy(qEvent.payloadStorage, data, payload.size);
  }

  // Notify OnPublish subscribers immediately on the caller's stack.
  {
    EventView view{};
    view.ptr = qEvent.payloadStorage;
    view.size = qEvent.payloadSize;
    view.isInline = false;
    PublishToSubscribers(code, qEvent.meta, view, SubscriptionDelivery::OnPublish);
  }

  std::lock_guard<std::mutex> lock(m_QueueMutex);
  m_EventQueue.push_back(qEvent);
}

std::size_t EventBus::DispatchQueued(std::size_t maxCount) noexcept {
  std::vector<QueuedEvent> events;

  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    std::size_t count = std::min(maxCount, m_EventQueue.size());
    if (count == 0)
      return 0;

    events.reserve(count);
    events.insert(events.end(), m_EventQueue.begin(),
                  m_EventQueue.begin() + count);
    m_EventQueue.erase(m_EventQueue.begin(), m_EventQueue.begin() + count);
  }

  for (const auto &qEvent : events) {
    EventView view{};
    view.ptr = qEvent.payloadStorage;
    view.size = qEvent.payloadSize;
    view.isInline = false;
    PublishToSubscribers(qEvent.meta.code, qEvent.meta, view,
                         SubscriptionDelivery::Queued);
  }

  return events.size();
}

EventEmitter EventBus::CreateEmitter(u8 domain, u64 sender) noexcept {
  EventEmitter emitter{};
  emitter.domain = domain;
  emitter.sender = sender;
  emitter.capability = m_CapabilitySecret ^ static_cast<u64>(domain);
  return emitter;
}

bool EventBus::ValidateEmitter(const EventEmitter &emitter,
                               u8 expectedDomain) const noexcept {
  if (emitter.domain != expectedDomain)
    return false;

  u64 expectedCapability =
      m_CapabilitySecret ^ static_cast<u64>(emitter.domain);
  return emitter.capability == expectedCapability;
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta &meta,
                                    EventView payload,
                                    SubscriptionDelivery deliveryFilter) {
  auto it = m_Subscribers.find(code);
  if (it == m_Subscribers.end())
    return;

  for (const auto &sub : it->second) {
    if (sub.delivery != deliveryFilter)
      continue;
    if (sub.callback) {
      sub.callback(sub.user, meta, payload);
    }
  }
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta &meta,
                                    EventView payload) {
  auto it = m_Subscribers.find(code);
  if (it == m_Subscribers.end())
    return;

  for (const auto &sub : it->second) {
    if (sub.callback) {
      sub.callback(sub.user, meta, payload);
    }
  }
}

} // namespace gecko::runtime
