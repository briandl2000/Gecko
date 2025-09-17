#include "Core/Event.h"

#include "Core/Logger.h"

#include <string>
#include <vector>

namespace Gecko { namespace Event 
{

  namespace
    {
    // TODO: create a system that will block events on systems on a lower level.
    class Event
    {
    public:
      Event() {};
      ~Event() {};

      void AddListener(EventCallbackInterface* eventCallback)
      {
        for (EventCallbackInterface* other : m_EventCallbacks)
        {
          if (*eventCallback == *other)
          {
            LOG_WARN("Event callback already exist in this event");
            return;
          }
        }
        m_EventCallbacks.push_back(eventCallback);
      }

      void RemoveListener(EventCallbackInterface* eventCallback)
      {
        for (CallbackArray::iterator it = m_EventCallbacks.begin(); it != m_EventCallbacks.end(); it++)
        {
          if (*eventCallback == *(*it))
          {
            m_EventCallbacks.erase(it);
            return;
          }
        }
        LOG_WARN("Event doesn't exist on this event!");
      }

      void Fire(const EventData& data)
      {
        for (EventCallbackInterface* eventCallback : m_EventCallbacks)
        {
          (*eventCallback)(data);
        }
      }

    private:
      using CallbackArray = std::vector<EventCallbackInterface*>;
      CallbackArray m_EventCallbacks;
    };

    constexpr u32 MAX_MESSAGE_CODES = 16384;

    struct EventSystemState
    {
      Event registeredEvents[MAX_MESSAGE_CODES];
    };

    static Scope<EventSystemState> s_State;
  }

  bool Init()
  {
    s_State = CreateScope<EventSystemState>();

    return true;
  }

  void Shutdown()
  {
    s_State = nullptr;
  }

  void EventRegister(u32 code, EventCallbackInterface* eventCallback)
  {
    if (!s_State)
    {
      LOG_WARN("Event system not initialized!");
      return;
    }
    s_State->registeredEvents[code].AddListener(eventCallback);
  }

  void EventUnregister(u32 code, EventCallbackInterface* eventCallback)
  {
    if (!s_State)
    {
      LOG_WARN("Event system not initialized!");
      return;
    }
    s_State->registeredEvents[code].RemoveListener(eventCallback);
  }

  void FireEvent(u32 code, const EventData& data)
  {
    if (!s_State)
    {
      LOG_WARN("Event system not initialized!");
      return;
    }
    EventData SendData = data;
    SendData.Code = code;
    s_State->registeredEvents[code].Fire(SendData);
  }

} }
