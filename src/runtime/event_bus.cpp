#include "gecko/runtime/event_bus.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/random.h"
#include "private/labels.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gecko::runtime {

struct EventBus::Impl
{
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

  std::unordered_map<EventCode, std::vector<Subscriber>> Subscribers;
  std::mutex SubscribersMutex;
  std::deque<QueuedEvent> EventQueue;
  std::mutex QueueMutex;
  std::atomic<u64> NextSubscriptionId {1};
  std::atomic<u64> NextSequence {0};
  u64 CapabilitySecret {0};
  std::unordered_set<u64> RegisteredModules;
  std::mutex ModulesMutex;
  bool Ready {false};
};

EventBus::EventBus() : m_Impl(new (::std::nothrow) Impl())
{}

EventBus::~EventBus() = default;

bool EventBus::Init() noexcept
{
  GECKO_SCOPE(runtime::labels::General);
  if (!m_Impl)
    return false;

  m_Impl->CapabilitySecret = RandomU64();

  m_Impl->Subscribers.clear();
  m_Impl->EventQueue.clear();
  m_Impl->RegisteredModules.clear();

  m_Impl->Subscribers.reserve(16);
  m_Impl->RegisteredModules.reserve(16);
  m_Impl->NextSubscriptionId.store(1, std::memory_order_relaxed);
  m_Impl->NextSequence.store(0, std::memory_order_relaxed);
  m_Impl->Ready = true;

  return true;
}

void EventBus::Shutdown() noexcept
{
  GECKO_SCOPE(runtime::labels::General);
  if (!m_Impl)
    return;

  {
    std::lock_guard<std::mutex> lock(m_Impl->SubscribersMutex);
    m_Impl->Subscribers.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_Impl->QueueMutex);
    m_Impl->EventQueue.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_Impl->ModulesMutex);
    m_Impl->RegisteredModules.clear();
  }
  m_Impl->Ready = false;
}

bool EventBus::RegisterModule(u64 moduleId) noexcept
{
  if (!m_Impl)
    return false;

  std::lock_guard<std::mutex> lock(m_Impl->ModulesMutex);

  if (!m_Impl->Ready)
    return false;

  if (m_Impl->RegisteredModules.find(moduleId) != m_Impl->RegisteredModules.end())
  {
    // Module already registered - this is a warning condition
    return false;
  }

  m_Impl->RegisteredModules.insert(moduleId);
  return true;
}

void EventBus::UnregisterModule(u64 moduleId) noexcept
{
  if (!m_Impl)
    return;

  std::lock_guard<std::mutex> lock(m_Impl->ModulesMutex);
  if (m_Impl->Ready)
    m_Impl->RegisteredModules.erase(moduleId);
}

EventSubscription EventBus::Subscribe(EventCode code, CallbackFn fn, void* user, SubscriptionOptions options) noexcept
{
  GECKO_SCOPE(runtime::labels::General);
  GECKO_ASSERT(fn && "Callback cannot be null");

  if (!m_Impl)
    return {};

  u64 id = m_Impl->NextSubscriptionId.fetch_add(1, std::memory_order_relaxed);
  GECKO_TRACE(runtime::labels::General, "Creating subscription ID=%llu for event code %u", (unsigned long long)id,
              code);

  Impl::Subscriber sub {};
  sub.id = id;
  sub.callback = fn;
  sub.user = user;
  sub.delivery = options.delivery;

  {
    std::lock_guard<std::mutex> lock(m_Impl->SubscribersMutex);
    if (m_Impl->Ready)
      m_Impl->Subscribers[code].push_back(sub);
  }

  EventSubscription subscription {};
  subscription.SetSubscriptionData(this, id);
  return subscription;
}

void EventBus::Unsubscribe(u64 id) noexcept
{
  if (id == 0)
    return;

  if (!m_Impl)
    return;

  std::lock_guard<std::mutex> lock(m_Impl->SubscribersMutex);
  if (!m_Impl->Ready)
    return;

  for (auto& [code, subscribers] : m_Impl->Subscribers)
  {
    auto it =
        std::find_if(subscribers.begin(), subscribers.end(), [id](const Impl::Subscriber& s) { return s.id == id; });
    if (it != subscribers.end())
    {
      subscribers.erase(it);
      return;
    }
  }
}

void EventBus::Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept
{
  GECKO_SCOPE_NAMED(runtime::labels::General, "SendEvent");
  if (!m_Impl)
    return;

  [[maybe_unused]] const u32 codeModuleHash = GetEventModule(code);
  [[maybe_unused]] const u32 emitterModuleHash = static_cast<u32>(emitter.moduleId >> 32);
  GECKO_ASSERT(codeModuleHash == emitterModuleHash && "Event code module mismatch with emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId) && "Invalid emitter capability");
  GECKO_ASSERT(payload.size <= sizeof(Impl::QueuedEvent::payloadStorage) && "Payload too large for queue");

  GECKO_TRACE(runtime::labels::General, "Sending event code=%u, moduleId=%llu, size=%zu", code,
              (unsigned long long)emitter.moduleId, payload.size);

  Impl::QueuedEvent qEvent {};
  qEvent.meta.code = code;
  qEvent.meta.moduleId = emitter.moduleId;
  qEvent.meta.sender = emitter.sender;
  qEvent.meta.seq = m_Impl->NextSequence.fetch_add(1, std::memory_order_relaxed);
  qEvent.payloadSize = payload.size;

  const void* data = payload.Data();
  if (data && payload.size > 0)
  {
    std::memcpy(qEvent.payloadStorage, data, payload.size);
  }

  // Notify Immediate subscribers on the caller's stack.
  {
    EventView view {qEvent.payloadStorage, qEvent.payloadSize};
    NotifySubscribers(code, qEvent.meta, view, SubscriptionDelivery::Immediate);
  }

  std::lock_guard<std::mutex> lock(m_Impl->QueueMutex);
  if (m_Impl->Ready)
    m_Impl->EventQueue.push_back(qEvent);
}

::gecko::usize EventBus::Dispatch(::gecko::usize maxCount) noexcept
{
  GECKO_SCOPE(runtime::labels::General);
  if (!m_Impl)
    return 0;

  std::vector<Impl::QueuedEvent> events;

  {
    std::lock_guard<std::mutex> lock(m_Impl->QueueMutex);
    if (!m_Impl->Ready)
      return 0;

    std::size_t count = std::min(maxCount, m_Impl->EventQueue.size());
    if (count == 0)
      return 0;

    GECKO_TRACE(runtime::labels::General, "Dispatching %zu queued events", count);

    events.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
      events.push_back(std::move(m_Impl->EventQueue.front()));
      m_Impl->EventQueue.pop_front();
    }
  }

  for (const auto& qEvent : events)
  {
    GECKO_PROFILE_NAMED(runtime::labels::General, "Dispatch::Notify");
    EventView view {qEvent.payloadStorage, qEvent.payloadSize};
    NotifySubscribers(qEvent.meta.code, qEvent.meta, view, SubscriptionDelivery::Queued);
  }

  return events.size();
}

EventEmitter EventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  EventEmitter emitter {};
  emitter.moduleId = moduleId;
  emitter.sender = sender;
  emitter.capability = (m_Impl ? m_Impl->CapabilitySecret : 0) ^ moduleId;
  return emitter;
}

bool EventBus::ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept
{
  if (emitter.moduleId != expectedModuleId)
    return false;

  if (!m_Impl)
    return false;

  u64 expectedCapability = m_Impl->CapabilitySecret ^ emitter.moduleId;
  return emitter.capability == expectedCapability;
}

void EventBus::NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload,
                                 SubscriptionDelivery deliveryFilter)
{
  GECKO_PROFILE_NAMED(runtime::labels::General, "EventBus::NotifySubscribers");
  if (!m_Impl)
    return;

  std::vector<Impl::Subscriber> subscribers;
  {
    GECKO_PROFILE_NAMED(runtime::labels::General, "Notify::CopySubs");
    std::lock_guard<std::mutex> lock(m_Impl->SubscribersMutex);
    if (!m_Impl->Ready)
      return;

    auto it = m_Impl->Subscribers.find(code);
    if (it == m_Impl->Subscribers.end())
      return;
    subscribers = it->second;
  }

  for (const auto& sub : subscribers)
  {
    if (sub.delivery != deliveryFilter)
      continue;
    if (sub.callback)
    {
      GECKO_PROFILE_NAMED(runtime::labels::General, "Notify::Callback");
      sub.callback(sub.user, meta, payload);
    }
  }
}

}  // namespace gecko::runtime
