#include "gecko/core/events.h"
#include "gecko/core/assert.h"
#include "gecko/core/labels.h"

namespace gecko {

void EventSubscription::Reset() {
  if (m_Bus && m_Id != 0) {
    m_Bus->Unsubscribe(m_Id);
  }
  m_Bus = nullptr;
  m_Id = 0;
}

EventEmitter CreateEmitterForModule(Label moduleLabel, u64 sender) {
  if (!moduleLabel.IsValid()) {
    GECKO_ASSERT(false && "Invalid module label!");
    return {};
  }
  return CreateEmitter(moduleLabel.Id, sender);
}

} // namespace gecko
