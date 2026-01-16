#include "gecko/core/events.h"

#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "labels.h"

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

}  // namespace gecko
