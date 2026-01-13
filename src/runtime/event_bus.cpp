#include "gecko/runtime/event_bus.h"
#include "gecko/core/assert.h"
#include "gecko/core/random.h"
#include <algorithm>
#include <cstring>

namespace gecko::runtime {

EventBus::EventBus() {}

EventBus::~EventBus() { m_Subscribers.clear(); }

bool EventBus::Init() noexcept {
  m_CapabilitySecret = RandomU64();
  return true;
}

void EventBus::Shutdown() noexcept {
  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    m_Subscribers.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_EventQueue.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_ModulesMutex);
    m_RegisteredModules.clear();
  }
}

bool EventBus::RegisterModule(u64 moduleId) noexcept {
  std::lock_guard<std::mutex> lock(m_ModulesMutex);

  if (m_RegisteredModules.find(moduleId) != m_RegisteredModules.end()) {
    // Module already registered - this is a warning condition
    return false;
  }

  m_RegisteredModules.insert(moduleId);
  return true;
}

void EventBus::UnregisterModule(u64 moduleId) noexcept {
  std::lock_guard<std::mutex> lock(m_ModulesMutex);
  m_RegisteredModules.erase(moduleId);
}

EventSubscription EventBus::Subscribe(EventCode code, CallbackFn fn, void *user,
                                      SubscriptionOptions options) noexcept {
  GECKO_ASSERT(fn && "Callback cannot be null");

  u64 id = m_NextSubscriptionId.fetch_add(1, std::memory_order_relaxed);

  Subscriber sub{};
  sub.id = id;
  sub.callback = fn;
  sub.user = user;
  sub.delivery = options.delivery;

  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    m_Subscribers[code].push_back(sub);
  }

  EventSubscription subscription{};
  subscription.SetSubscriptionData(this, id);
  return subscription;
}

void EventBus::Unsubscribe(u64 id) noexcept {
  if (id == 0)
    return;

  std::lock_guard<std::mutex> lock(m_SubscribersMutex);
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
  [[maybe_unused]] const u32 codeModuleHash = GetEventModule(code);
  [[maybe_unused]] const u32 emitterModuleHash = static_cast<u32>(emitter.moduleId >> 32);
  GECKO_ASSERT(codeModuleHash == emitterModuleHash &&
               "Event code module mismatch with emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId) &&
               "Invalid emitter capability");

  EventMeta meta{};
  meta.code = code;
  meta.moduleId = emitter.moduleId;
  meta.sender = emitter.sender;
  meta.seq = m_NextSequence.fetch_add(1, std::memory_order_relaxed);

  PublishToSubscribers(code, meta, payload);
}

void EventBus::Enqueue(const EventEmitter &emitter, EventCode code,
                       EventView payload) noexcept {
  [[maybe_unused]] const u32 codeModuleHash = GetEventModule(code);
  [[maybe_unused]] const u32 emitterModuleHash = static_cast<u32>(emitter.moduleId >> 32);
  GECKO_ASSERT(codeModuleHash == emitterModuleHash &&
               "Event code module mismatch with emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId) &&
               "Invalid emitter capability");
  GECKO_ASSERT(payload.size <= sizeof(QueuedEvent::payloadStorage) &&
               "Payload too large for queue");

  QueuedEvent qEvent{};
  qEvent.meta.code = code;
  qEvent.meta.moduleId = emitter.moduleId;
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
    PublishToSubscribers(code, qEvent.meta, view,
                         SubscriptionDelivery::OnPublish);
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
    for (std::size_t i = 0; i < count; ++i) {
      events.push_back(std::move(m_EventQueue.front()));
      m_EventQueue.pop_front();
    }
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

EventEmitter EventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept {
  EventEmitter emitter{};
  emitter.moduleId = moduleId;
  emitter.sender = sender;
  emitter.capability = m_CapabilitySecret ^ moduleId;
  return emitter;
}

bool EventBus::ValidateEmitter(const EventEmitter &emitter,
                               u64 expectedModuleId) const noexcept {
  if (emitter.moduleId != expectedModuleId)
    return false;

  u64 expectedCapability = m_CapabilitySecret ^ emitter.moduleId;
  return emitter.capability == expectedCapability;
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta &meta,
                                    EventView payload,
                                    SubscriptionDelivery deliveryFilter) {
  std::vector<Subscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    auto it = m_Subscribers.find(code);
    if (it == m_Subscribers.end())
      return;
    subscribers = it->second;
  }

  for (const auto &sub : subscribers) {
    if (sub.delivery != deliveryFilter)
      continue;
    if (sub.callback) {
      sub.callback(sub.user, meta, payload);
    }
  }
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta &meta,
                                    EventView payload) {
  // Delegate to the overload that accepts a delivery filter, using a
  // default-constructed SubscriptionDelivery value.
  PublishToSubscribers(code, meta, payload, SubscriptionDelivery{});
}

} // namespace gecko::runtime
