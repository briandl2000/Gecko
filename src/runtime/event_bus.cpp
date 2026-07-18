#include "gecko/runtime/event_bus.h"

#include "gecko/core/assert.h"
#include "gecko/core/array.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/random.h"
#include "private/labels.h"

namespace gecko::runtime {

void EventBus::Lock() const noexcept
{
#if defined(_MSC_VER)
  while (_InterlockedExchange(reinterpret_cast<volatile long*>(&m_Lock), 1) != 0)
  {}
#else
  while (__atomic_exchange_n(&m_Lock, 1U, __ATOMIC_ACQUIRE) != 0)
  {}
#endif
}

void EventBus::Unlock() const noexcept
{
#if defined(_MSC_VER)
  (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&m_Lock), 0);
#else
  __atomic_store_n(&m_Lock, 0U, __ATOMIC_RELEASE);
#endif
}

bool EventBus::Init() noexcept
{
  GECKO_ASSERT(!m_Initialized, "EventBus already initialized");
  m_CapabilitySecret = RandomU64();
  m_Initialized = true;
  return true;
}

void EventBus::Shutdown() noexcept
{
  Lock();
  for (Subscriber& subscriber : m_Subscribers)
    subscriber = {};
  m_QueueRead = 0;
  m_QueueCount = 0;
  m_ModuleCount = 0;
  m_Initialized = false;
  Unlock();
}

bool EventBus::RegisterModule(u64 moduleId) noexcept
{
  Lock();
  for (u32 index = 0; index < m_ModuleCount; ++index)
  {
    if (m_Modules[index] == moduleId)
    {
      Unlock();
      return false;
    }
  }
  if (!m_Initialized || m_ModuleCount == MaxRegisteredModules)
  {
    Unlock();
    return false;
  }
  m_Modules[m_ModuleCount++] = moduleId;
  Unlock();
  return true;
}

void EventBus::UnregisterModule(u64 moduleId) noexcept
{
  Lock();
  for (u32 index = 0; index < m_ModuleCount; ++index)
  {
    if (m_Modules[index] != moduleId)
      continue;
    m_Modules[index] = m_Modules[--m_ModuleCount];
    break;
  }
  Unlock();
}

EventSubscription EventBus::Subscribe(EventCode code, CallbackFn callback, void* user,
                                      SubscriptionOptions options) noexcept
{
  GECKO_ASSERT(callback != nullptr, "Event callback cannot be null");
  Lock();
  Subscriber* destination = nullptr;
  for (Subscriber& subscriber : m_Subscribers)
  {
    if (!subscriber.Active)
    {
      destination = &subscriber;
      break;
    }
  }
  if (destination == nullptr || !m_Initialized)
  {
    Unlock();
    return {};
  }

  const u64 id = m_NextSubscriptionId++;
  *destination = Subscriber {
      .Code = code,
      .Id = id,
      .Callback = callback,
      .User = user,
      .Delivery = options.delivery,
      .Active = true,
  };
  Unlock();

  GECKO_TRACE(runtime::labels::General, "Creating subscription ID={} for event code {}", id, code);
  EventSubscription subscription;
  subscription.SetSubscriptionData(this, id);
  return subscription;
}

void EventBus::Unsubscribe(u64 id) noexcept
{
  if (id == 0)
    return;
  Lock();
  for (Subscriber& subscriber : m_Subscribers)
  {
    if (subscriber.Active && subscriber.Id == id)
    {
      subscriber = {};
      break;
    }
  }
  Unlock();
}

void EventBus::Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept
{
  GECKO_ASSERT(GetEventModule(code) == static_cast<u32>(emitter.moduleId >> 32U),
               "Event code module does not match emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.moduleId), "Invalid event emitter");
  GECKO_ASSERT(payload.size <= MaxPayloadSize, "Event payload exceeds fixed queue storage");

  QueuedEvent event {
      .Meta = {.code = code, .moduleId = emitter.moduleId, .sender = emitter.sender},
      .PayloadSize = payload.size,
  };
  Lock();
  event.Meta.seq = m_NextSequence++;
  Unlock();
  if (payload.ptr != nullptr)
  {
    const auto* source = static_cast<const u8*>(payload.ptr);
    for (u32 index = 0; index < payload.size; ++index)
      event.Payload[index] = source[index];
  }

  NotifySubscribers(code, event.Meta, EventView {event.Payload, event.PayloadSize},
                    SubscriptionDelivery::Immediate);

  Lock();
  if (m_QueueCount == MaxQueuedEvents)
  {
    Unlock();
    GECKO_WARN(runtime::labels::General, "Event queue full; dropping event {}", code);
    return;
  }
  const u32 writeIndex = (m_QueueRead + m_QueueCount) % MaxQueuedEvents;
  m_Queue[writeIndex] = event;
  ++m_QueueCount;
  Unlock();
}

usize EventBus::Dispatch(usize maxCount) noexcept
{
  usize dispatched = 0;
  while (dispatched < maxCount)
  {
    Lock();
    if (m_QueueCount == 0)
    {
      Unlock();
      break;
    }
    const QueuedEvent event = m_Queue[m_QueueRead];
    m_QueueRead = (m_QueueRead + 1U) % MaxQueuedEvents;
    --m_QueueCount;
    Unlock();

    NotifySubscribers(event.Meta.code, event.Meta, EventView {event.Payload, event.PayloadSize},
                      SubscriptionDelivery::Queued);
    ++dispatched;
  }
  return dispatched;
}

EventEmitter EventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  return EventEmitter {.moduleId = moduleId, .sender = sender, .capability = m_CapabilitySecret ^ moduleId};
}

bool EventBus::ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept
{
  if (emitter.moduleId != expectedModuleId || emitter.capability != (m_CapabilitySecret ^ emitter.moduleId))
    return false;
  Lock();
  bool registered = false;
  for (u32 index = 0; index < m_ModuleCount; ++index)
    registered |= m_Modules[index] == emitter.moduleId;
  Unlock();
  return registered;
}

void EventBus::NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload,
                                 SubscriptionDelivery delivery) noexcept
{
  Array<Subscriber> matches;
  Lock();
  for (const Subscriber& subscriber : m_Subscribers)
  {
    if (subscriber.Active && subscriber.Code == code && subscriber.Delivery == delivery)
      matches.PushBack(subscriber);
  }
  Unlock();

  for (const Subscriber& subscriber : matches)
    subscriber.Callback(subscriber.User, meta, payload);
}

}  // namespace gecko::runtime
