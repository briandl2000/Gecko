#include "gecko/core/services/events.h"

#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

namespace gecko {

void EventSubscription::Reset()
{
  GECKO_PROF_FUNC(core::labels::Events);

  if (m_Bus && m_Id != 0)
  {
    GECKO_TRACE(core::labels::Events,
                "Unsubscribing event subscription ID=%llu",
                (unsigned long long)m_Id);
    m_Bus->Unsubscribe(m_Id);
  }
  m_Bus = nullptr;
  m_Id = 0;
}

EventEmitter CreateEmitterForModule(Label moduleLabel, u64 sender)
{
  GECKO_PROF_FUNC(core::labels::Events);

  if (!moduleLabel.IsValid())
  {
    GECKO_ERROR(core::labels::Events,
                "CreateEmitterForModule: invalid module label");
    GECKO_ASSERT(false && "Invalid module label!");
    return {};
  }

  GECKO_TRACE(core::labels::Events,
              "Creating emitter for module '%s' (sender=%llu)",
              moduleLabel.Name ? moduleLabel.Name : "<unnamed>",
              (unsigned long long)sender);
  return CreateEmitter(moduleLabel.Id, sender);
}

EventSubscription NullEventBus::Subscribe(EventCode, CallbackFn, void*,
                                          SubscriptionOptions) noexcept
{
  return {};
}

void NullEventBus::PublishImmediate(const EventEmitter&, EventCode,
                                    EventView) noexcept
{}

void NullEventBus::Enqueue(const EventEmitter&, EventCode, EventView) noexcept
{}

std::size_t NullEventBus::DispatchQueued(std::size_t) noexcept
{
  return 0;
}

bool NullEventBus::RegisterModule(u64) noexcept
{
  return true;
}

void NullEventBus::UnregisterModule(u64) noexcept
{}

EventEmitter NullEventBus::CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  EventEmitter e {};
  e.moduleId = moduleId;
  e.sender = sender;
  e.capability = 0;
  return e;
}

bool NullEventBus::ValidateEmitter(const EventEmitter&, u64) const noexcept
{
  return true;
}

void NullEventBus::Unsubscribe(u64) noexcept
{}

bool NullEventBus::Init() noexcept
{
  return true;
}

void NullEventBus::Shutdown() noexcept
{}

}  // namespace gecko
