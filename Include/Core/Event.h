#pragma once
#include "Defines.h"
#include "Core/Logger.h"

namespace Gecko { namespace Event 
{

  enum SystemEvent : u32
  {
    CODE_NONE = 0x00,
    CODE_WINDOW_CLOSED = 0x01,
    CODE_KEY_PRESSED = 0x02,
    CODE_KEY_RELEASED = 0x03,
    CODE_BUTTON_PRESSED = 0x04,
    CODE_BUTTON_RELEASED = 0x05,
    CODE_MOUSE_MOVED = 0x06,
    CODE_MOUSE_WHEEL = 0x07,
    CODE_RESIZED = 0x08,

    MAX_EVENT_CODE = 0xFF
  };

  struct EventData
  {
    u32 Code{ SystemEvent::CODE_NONE };
    void* Sender;
    union
    {
      i64 i64[2];
      u64 u64[2];
      f64 f64[2];

      i32 i32[4];
      u32 u32[4];
      f32 f32[4];

      i16 i16[8];
      u16 u16[8];

      i8 i8[16];
      u8 u8[16];

      char c[16];
    } Data;
  };

  bool Init();
  void Shutdown();
  void FireEvent(u32 code, const EventData& data);

  class EventCallbackInterface;
  void EventRegister(u32 code, EventCallbackInterface* eventCallback);
  void EventUnregister(u32 code, EventCallbackInterface* eventCallback);

  class EventCallbackInterface
  {
  public:
    EventCallbackInterface() = default;
    virtual ~EventCallbackInterface() {};
    virtual bool operator() (const EventData& data) = 0;
    virtual bool operator == (const EventCallbackInterface& other) const = 0;
  };

  template<typename T>
  class EventListener;

  template<typename T>
  class EventCallback : public EventCallbackInterface
  {
  public:
    EventCallback(u32 code, T* listener, bool (T::* callback)(const EventData& data))
      : m_Callback(callback)
      , m_Listener(listener)
      , m_Code(code)
    {
      EventRegister(m_Code, this);
    }

    virtual ~EventCallback() override
    {
      EventUnregister(m_Code, this);
    }

    virtual bool operator () (const EventData& data) override
    {
      return (m_Listener->*m_Callback)(data);
    }

    virtual bool operator == (const EventCallbackInterface& other) const override
    {
      const EventCallback<T>* otherEventCallback = reinterpret_cast<const EventCallback<T>*>(&other);
      
      if (otherEventCallback == nullptr)
        return false;
      
      return (this->m_Callback == otherEventCallback->m_Callback) &&
             (this->m_Listener == otherEventCallback->m_Listener) &&
             (this->m_Code == otherEventCallback->m_Code);
    }

  private:
    bool (T::* m_Callback)(const EventData& data);
    T* m_Listener;
    u32 m_Code;
    friend class EventListener<T>;
  };

  template<typename T>
  class EventListener
  {
  protected:
    EventListener() = default;
    virtual ~EventListener() = default;

    void AddEventListener(u32 code, bool(T::* callback)(const EventData& data))
    {
      for (auto it = m_EventListeners.begin(); it != m_EventListeners.end(); it++)
      {
        if ((*it)->m_Code == code && (*it)->m_Listener == reinterpret_cast<T*>(this) && (*it)->m_Callback == callback)
        {
          LOG_WARN("Event callback already exist in this EventListener");
          return;
        }
      }

      m_EventListeners.push_back(
        CreateRef<EventCallback<T>>(code, reinterpret_cast<T*>(this), callback)
      );
    }
    void RemoveEventListener(u32 code, bool(T::* callback)(const EventData& data))
    {
      for (auto it = m_EventListeners.begin(); it != m_EventListeners.end(); it++)
      {
        if ((*it)->m_Code == code && (*it)->m_Listener == reinterpret_cast<T*>(this) && (*it)->m_Callback == callback)
        {
          m_EventListeners.erase(it);
          return;
        }
      }
      LOG_WARN("Event doesn't exist on this EventListener!");
    }

  private:
    std::vector<Ref<EventCallback<T>>> m_EventListeners;
  };

} }
