#include "gecko/runtime/event_bus.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/random.h"
#include "private/labels.h"

#include <algorithm>
#include <cstring>

namespace gecko::runtime {

EventBus::EventBus()
{}

EventBus::~EventBus()
{
  // unique_ptr automatically cleans up
}

bool EventBus::Init() noexcept
{
  GECKO_FUNC(runtime::labels::General);
  m_CapabilitySecret = RandomU64();

  // Allocate containers now that allocator service is installed
  m_Subscribers = std::make_unique<
      std::unordered_map<EventCode, std::vector<Subscriber>>>();
  m_EventQueue = std::make_unique<std::deque<QueuedEvent>>();
  m_RegisteredModules = std::make_unique<std::unordered_set<u64>>();

  m_Subscribers->reserve(16);
  m_RegisteredModules->reserve(16);

  return true;
}

void EventBus::Shutdown() noexcept
{
  GECKO_FUNC(runtime::labels::General);

  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    m_Subscribers.reset();
  }
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_EventQueue.reset();
  }
  {
    std::lock_guard<std::mutex> lock(m_ModulesMutex);
    m_RegisteredModules.reset();
  }
}

bool EventBus::RegisterModule(u64 moduleId) noexcept
{
  std::lock_guard<std::mutex> lock(m_ModulesMutex);

  if (!m_RegisteredModules)
    return false;

  if (m_RegisteredModules->find(moduleId) != m_RegisteredModules->end())
  {
    // Module already registered - this is a warning condition
    return false;
  }

  m_RegisteredModules->insert(moduleId);
  return true;
}

void EventBus::UnregisterModule(u64 moduleId) noexcept
{
  std::lock_guard<std::mutex> lock(m_ModulesMutex);
  if (m_RegisteredModules)
    m_RegisteredModules->erase(moduleId);
}

EventSubscription EventBus::Subscribe(EventCode code, CallbackFn fn, void* user,
                                      SubscriptionOptions options) noexcept
{
  GECKO_FUNC(runtime::labels::General);
  GECKO_ASSERT(fn && "Callback cannot be null");

  u64 id = m_NextSubscriptionId.fetch_add(1, std::memory_order_relaxed);
  GECKO_TRACE(runtime::labels::General,
              "Creating subscription ID=%llu for event code %u",
              (unsigned long long)id, code);

  Subscriber sub {};
  sub.id = id;
  sub.callback = fn;
  sub.user = user;
  sub.delivery = options.delivery;

  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    if (m_Subscribers)
      (*m_Subscribers)[code].push_back(sub);
  }

  EventSubscription subscription {};
  subscription.SetSubscriptionData(this, id);
  return subscription;
}

void EventBus::Unsubscribe(u64 id) noexcept
{
  if (id == 0)
    return;

  std::lock_guard<std::mutex> lock(m_SubscribersMutex);
  if (!m_Subscribers)
    return;

  for (auto& [code, subscribers] : *m_Subscribers)
  {
    auto it = std::find_if(subscribers.begin(), subscribers.end(),
                           [id](const Subscriber& s) { return s.id == id; });
    if (it != subscribers.end())
    {
      subscribers.erase(it);
      return;
    }
  }
}

void EventBus::PublishImmediate(const EventEmitter& emitter, EventCode code,
                                EventView payload) noexcept
{
  GECKO_SCOPE_NAMED(runtime::labels::General, "PublishImmediate");

  [[maybe_unused]] const u32 codeModuleHash = GetEventModule(code);
  [[maybe_unused]] const u32 emitterModuleHash =
      static_cast<u32>(emitter.moduleId >> 32);
  GECKO_ASSERT(codeModuleHash == emitterModuleHash &&
               "Event code module mismatch with emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId) &&
               "Invalid emitter capability");

  GECKO_TRACE(runtime::labels::General,
              "Publishing immediate event code=%u, moduleId=%llu", code,
              (unsigned long long)emitter.moduleId);

  EventMeta meta {};
  meta.code = code;
  meta.moduleId = emitter.moduleId;
  meta.sender = emitter.sender;
  meta.seq = m_NextSequence.fetch_add(1, std::memory_order_relaxed);

  PublishToSubscribers(code, meta, payload);
}

void EventBus::Enqueue(const EventEmitter& emitter, EventCode code,
                       EventView payload) noexcept
{
  GECKO_SCOPE_NAMED(runtime::labels::General, "EnqueueEvent");

  [[maybe_unused]] const u32 codeModuleHash = GetEventModule(code);
  [[maybe_unused]] const u32 emitterModuleHash =
      static_cast<u32>(emitter.moduleId >> 32);
  GECKO_ASSERT(codeModuleHash == emitterModuleHash &&
               "Event code module mismatch with emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId) &&
               "Invalid emitter capability");
  GECKO_ASSERT(payload.size <= sizeof(QueuedEvent::payloadStorage) &&
               "Payload too large for queue");

  GECKO_TRACE(runtime::labels::General,
              "Enqueuing event code=%u, moduleId=%llu, size=%zu", code,
              (unsigned long long)emitter.moduleId, payload.size);

  QueuedEvent qEvent {};
  qEvent.meta.code = code;
  qEvent.meta.moduleId = emitter.moduleId;
  qEvent.meta.sender = emitter.sender;
  qEvent.meta.seq = m_NextSequence.fetch_add(1, std::memory_order_relaxed);
  qEvent.payloadSize = payload.size;

  const void* data = payload.Data();
  if (data && payload.size > 0)
  {
    std::memcpy(qEvent.payloadStorage, data, payload.size);
  }

  // Notify OnPublish subscribers immediately on the caller's stack.
  {
    EventView view {qEvent.payloadStorage, qEvent.payloadSize};
    PublishToSubscribers(code, qEvent.meta, view,
                         SubscriptionDelivery::OnPublish);
  }

  std::lock_guard<std::mutex> lock(m_QueueMutex);
  if (m_EventQueue)
    m_EventQueue->push_back(qEvent);
}

std::size_t EventBus::DispatchQueued(std::size_t maxCount) noexcept
{
  GECKO_FUNC(runtime::labels::General);

  std::vector<QueuedEvent> events;

  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    if (!m_EventQueue)
      return 0;

    std::size_t count = std::min(maxCount, m_EventQueue->size());
    if (count == 0)
      return 0;

    GECKO_TRACE(runtime::labels::General, "Dispatching %zu queued events",
                count);

    events.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
      events.push_back(std::move(m_EventQueue->front()));
      m_EventQueue->pop_front();
    }
  }

  for (const auto& qEvent : events)
  {
    EventView view {qEvent.payloadStorage, qEvent.payloadSize};
    PublishToSubscribers(qEvent.meta.code, qEvent.meta, view,
                         SubscriptionDelivery::Queued);
  }

  return events.size();
}

EventEmitter EventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  EventEmitter emitter {};
  emitter.moduleId = moduleId;
  emitter.sender = sender;
  emitter.capability = m_CapabilitySecret ^ moduleId;
  return emitter;
}

bool EventBus::ValidateEmitter(const EventEmitter& emitter,
                               u64 expectedModuleId) const noexcept
{
  if (emitter.moduleId != expectedModuleId)
    return false;

  u64 expectedCapability = m_CapabilitySecret ^ emitter.moduleId;
  return emitter.capability == expectedCapability;
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta& meta,
                                    EventView payload,
                                    SubscriptionDelivery deliveryFilter)
{
  std::vector<Subscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    if (!m_Subscribers)
      return;

    auto it = m_Subscribers->find(code);
    if (it == m_Subscribers->end())
      return;
    subscribers = it->second;
  }

  for (const auto& sub : subscribers)
  {
    if (sub.delivery != deliveryFilter)
      continue;
    if (sub.callback)
    {
      sub.callback(sub.user, meta, payload);
    }
  }
}

void EventBus::PublishToSubscribers(EventCode code, const EventMeta& meta,
                                    EventView payload)
{
  // Notify ALL subscribers regardless of delivery type (used by
  // PublishImmediate)
  std::vector<Subscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(m_SubscribersMutex);
    if (!m_Subscribers)
      return;

    auto it = m_Subscribers->find(code);
    if (it == m_Subscribers->end())
      return;
    subscribers = it->second;
  }

  for (const auto& sub : subscribers)
  {
    if (sub.callback)
    {
      sub.callback(sub.user, meta, payload);
    }
  }
}

}  // namespace gecko::runtime
