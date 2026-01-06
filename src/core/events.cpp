#include "gecko/core/events.h"

namespace gecko {

void EventSubscription::Reset() {
  if (m_Bus && m_Id != 0) {
    m_Bus->Unsubscribe(m_Id);
  }
  m_Bus = nullptr;
  m_Id = 0;
}

} // namespace gecko
