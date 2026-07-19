#include "private/event_bus.h"

#include "gecko/core/containers/array.h"
#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/random.h"
#include "private/labels.h"

namespace gecko::runtime {

bool EventBus::Init() noexcept
{
  GECKO_ASSERT(!m_Initialized, "EventBus already initialized");
  m_CapabilitySecret = RandomU64();
  m_Initialized = true;
  return true;
}

void EventBus::Shutdown() noexcept
{
  LockGuard lock(m_Mutex);
  for (Subscriber& subscriber : m_Subscribers)
    subscriber = {};
  m_QueueRead = 0;
  m_QueueCount = 0;
  m_ModuleCount = 0;
  m_Initialized = false;
}

bool EventBus::RegisterModule(u64 moduleId) noexcept
{
  LockGuard lock(m_Mutex);
  for (u32 index = 0; index < m_ModuleCount; ++index)
  {
    if (m_Modules[index] == moduleId)
    {
      return false;
    }
  }
  if (!m_Initialized || m_ModuleCount == MaxRegisteredModules)
  {
    return false;
  }
  m_Modules[m_ModuleCount++] = moduleId;
  return true;
}

void EventBus::UnregisterModule(u64 moduleId) noexcept
{
  LockGuard lock(m_Mutex);
  for (u32 index = 0; index < m_ModuleCount; ++index)
  {
    if (m_Modules[index] != moduleId)
      continue;
    m_Modules[index] = m_Modules[--m_ModuleCount];
    break;
  }
}

EventSubscription EventBus::Subscribe(EventCode code, EventCallbackFn callback, void* user,
                                      SubscriptionOptions options) noexcept
{
  GECKO_ASSERT(callback != nullptr, "Event callback cannot be null");
  u64 id = 0;
  {
    LockGuard lock(m_Mutex);
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
      return {};

    id = m_NextSubscriptionId++;
    *destination = Subscriber {
        .Code = code,
        .Id = id,
        .Callback = callback,
        .User = user,
        .Delivery = options.Delivery,
        .Active = true,
    };
  }
  GECKO_TRACE(runtime::labels::General, "Creating subscription ID={} for event code {}", id, code);
  return EventSubscription {id};
}

void EventBus::Unsubscribe(u64 id) noexcept
{
  if (id == 0)
    return;
  LockGuard lock(m_Mutex);
  for (Subscriber& subscriber : m_Subscribers)
  {
    if (subscriber.Active && subscriber.Id == id)
    {
      subscriber = {};
      break;
    }
  }
}

void EventBus::Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept
{
  GECKO_ASSERT(GetEventModule(code) == static_cast<u32>(emitter.ModuleId >> 32U),
               "Event code module does not match emitter module");
  GECKO_ASSERT(ValidateEmitter(emitter, emitter.ModuleId), "Invalid event emitter");
  if (payload.Size > MaxPayloadSize || (payload.Size != 0 && payload.Bytes == nullptr))
  {
    GECKO_ERROR(runtime::labels::General, "Dropping event {} with invalid payload ({} bytes, data={})", code,
                payload.Size, payload.Bytes);
    return;
  }

  QueuedEvent event {
      .Meta = {.Code = code, .ModuleId = emitter.ModuleId, .Sender = emitter.Sender},
      .PayloadSize = payload.Size,
  };
  {
    LockGuard lock(m_Mutex);
    event.Meta.Sequence = m_NextSequence++;
  }
  if (payload.Bytes != nullptr)
  {
    const auto* source = static_cast<const u8*>(payload.Bytes);
    for (u32 index = 0; index < payload.Size; ++index)
      event.Payload[index] = source[index];
  }

  NotifySubscribers(code, event.Meta, EventView {event.Payload, event.PayloadSize}, SubscriptionDelivery::Immediate);

  bool queued = false;
  {
    LockGuard lock(m_Mutex);
    if (m_QueueCount != MaxQueuedEvents)
    {
      const u32 writeIndex = (m_QueueRead + m_QueueCount) % MaxQueuedEvents;
      m_Queue[writeIndex] = event;
      ++m_QueueCount;
      queued = true;
    }
  }
  if (!queued)
    GECKO_WARN(runtime::labels::General, "Event queue full; dropping event {}", code);
}

usize EventBus::Dispatch(usize maxCount) noexcept
{
  usize dispatched = 0;
  while (dispatched < maxCount)
  {
    QueuedEvent event {};
    {
      LockGuard lock(m_Mutex);
      if (m_QueueCount == 0)
        break;
      event = m_Queue[m_QueueRead];
      m_QueueRead = (m_QueueRead + 1U) % MaxQueuedEvents;
      --m_QueueCount;
    }

    NotifySubscribers(event.Meta.Code, event.Meta, EventView {event.Payload, event.PayloadSize},
                      SubscriptionDelivery::Queued);
    ++dispatched;
  }
  return dispatched;
}

EventEmitter EventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  return EventEmitter {.ModuleId = moduleId, .Sender = sender, .Capability = m_CapabilitySecret ^ moduleId};
}

bool EventBus::ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept
{
  if (emitter.ModuleId != expectedModuleId || emitter.Capability != (m_CapabilitySecret ^ emitter.ModuleId))
    return false;
  LockGuard lock(m_Mutex);
  bool registered = false;
  for (u32 index = 0; index < m_ModuleCount; ++index)
    registered |= m_Modules[index] == emitter.ModuleId;
  return registered;
}

void EventBus::NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload,
                                 SubscriptionDelivery delivery) noexcept
{
  Array<Subscriber> matches;
  {
    LockGuard lock(m_Mutex);
    for (const Subscriber& subscriber : m_Subscribers)
    {
      if (subscriber.Active && subscriber.Code == code && subscriber.Delivery == delivery)
        matches.PushBack(subscriber);
    }
  }

  for (const Subscriber& subscriber : matches)
    subscriber.Callback(subscriber.User, meta, payload);
}

}  // namespace gecko::runtime
