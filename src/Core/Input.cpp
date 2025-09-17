#include "Core/Input.h"

#include "Core/Event.h"

namespace Gecko { namespace Input 
{

  namespace
  {
    #define MAX_KEYS 256
    #define MAX_MOUSE_BUTTONS 5
    struct KeyboardState
    {
      bool Keys[MAX_KEYS]{ false };
    };

    struct MouseState
    {
      i16 X{ 0 };
      i16 Y{ 0 };
      i8 WheelDelta{0};
      bool Buttons[MAX_MOUSE_BUTTONS]{ false };
    };

    class InputState : protected Event::EventListener<InputState>
    {
    public:
      InputState() {};

      void Init();
      void Shutdown();
      void Update();

      KeyboardState CurrentKeyboard;
      KeyboardState PreviousKeyboard;
      MouseState CurrentMouse;
      MouseState PreviousMouse;

    private:
      bool OnKeyPressedEvent(const Event::EventData& data);
      bool OnKeyReleasedEvent(const Event::EventData& data);

      bool OnMouseButtonPressedEvent(const Event::EventData& data);
      bool OnMouseButtonReleasedEvent(const Event::EventData& data);

      bool OnMouseMoveEvent(const Event::EventData& data);
      bool OnMouseWheelMoveEvent(const Event::EventData& data);

      void ProcessKey(Key key, bool pressed);
      void ProcessButton(MouseButton button, bool pressed);
      void ProcessMouseMove(i32 x, i32 y);
      void ProcessMouseWheel(i32 zDelta);
    };

    static Scope<InputState> s_State;
  }

  bool Init()
  {
    s_State = CreateScope<InputState>();
    s_State->Init(); 
    return true;
  }

  void Shutdown()
  {
    s_State->Shutdown();
    s_State = nullptr;
  }

  void Update()
  {
    if (!s_State)
      return;
    s_State->Update();
  }

  bool IsKeyDown(Key key)
  {
    if (!s_State)
      return false;
    return s_State->CurrentKeyboard.Keys[static_cast<u32>(key)] == true;
  }

  bool IsKeyUp(Key key)
  {
    if (!s_State)
      return true;
    return s_State->CurrentKeyboard.Keys[static_cast<u32>(key)] == false;
  }

  bool WasKeyDown(Key key)
  {
    if (!s_State)
      return false;
    return s_State->PreviousKeyboard.Keys[static_cast<u32>(key)] == true;
  }

  bool WasKeyUp(Key key)
  {
    if (!s_State)
      return true;
    return s_State->PreviousKeyboard.Keys[static_cast<u32>(key)] == false;
  }

  bool IsButtonDown(MouseButton button)
  {
    if (!s_State)
      return false;
    return s_State->CurrentMouse.Buttons[static_cast<u32>(button)] == true;
  }

  bool IsButtonUp(MouseButton button)
  {
    if (!s_State)
      return true;
    return s_State->CurrentMouse.Buttons[static_cast<u32>(button)] == false;
  }

  bool WasButtonDown(MouseButton button)
  {
    if (!s_State)
      return false;
    return s_State->PreviousMouse.Buttons[static_cast<u32>(button)] == true;
  }

  bool WasButtonUp(MouseButton button)
  {
    if (!s_State)
      return true;
    return s_State->PreviousMouse.Buttons[static_cast<u32>(button)] == false;
  }

  void GetMousePosition(i32* x, i32* y)
  {
    if (!s_State)
    {
      *x = 0;
      *y = 0;
      return;
    }
    *x = s_State->CurrentMouse.X;
    *y = s_State->CurrentMouse.Y;
  }

  void GetPreviousMousePosition(i32* x, i32* y)
  {
    if (!s_State)
    {
      *x = 0;
      *y = 0;
      return;
    }
    *x = s_State->PreviousMouse.X;
    *y = s_State->PreviousMouse.Y;
  }

  namespace {

    void InputState::Init()
    {
      AddEventListener(Event::SystemEvent::CODE_KEY_PRESSED, &InputState::OnKeyPressedEvent);
      AddEventListener(Event::SystemEvent::CODE_KEY_RELEASED, &InputState::OnKeyReleasedEvent);
      AddEventListener(Event::SystemEvent::CODE_BUTTON_PRESSED, &InputState::OnMouseButtonPressedEvent);
      AddEventListener(Event::SystemEvent::CODE_BUTTON_RELEASED, &InputState::OnMouseButtonReleasedEvent);
      AddEventListener(Event::SystemEvent::CODE_MOUSE_MOVED, &InputState::OnMouseMoveEvent);
      AddEventListener(Event::SystemEvent::CODE_MOUSE_WHEEL, &InputState::OnMouseWheelMoveEvent);
    }

    void InputState::Shutdown()
    {
      RemoveEventListener(Event::SystemEvent::CODE_KEY_PRESSED, &InputState::OnKeyPressedEvent);
      RemoveEventListener(Event::SystemEvent::CODE_KEY_RELEASED, &InputState::OnKeyReleasedEvent);
      RemoveEventListener(Event::SystemEvent::CODE_BUTTON_PRESSED, &InputState::OnMouseButtonPressedEvent);
      RemoveEventListener(Event::SystemEvent::CODE_BUTTON_RELEASED, &InputState::OnMouseButtonReleasedEvent);
      RemoveEventListener(Event::SystemEvent::CODE_MOUSE_MOVED, &InputState::OnMouseMoveEvent);
      RemoveEventListener(Event::SystemEvent::CODE_MOUSE_WHEEL, &InputState::OnMouseWheelMoveEvent);
    }

    void InputState::Update()
    {
      memcpy(&PreviousKeyboard, &CurrentKeyboard, sizeof(KeyboardState));
      memcpy(&PreviousMouse, &CurrentMouse, sizeof(MouseState));
    }

    bool InputState::OnKeyPressedEvent(const Event::EventData& data)
    {
      ProcessKey(static_cast<Key>(data.Data.u32[0]), true);
      return false;
    }

    bool InputState::OnKeyReleasedEvent(const Event::EventData& data)
    {
      ProcessKey(static_cast<Key>(data.Data.u32[0]), false);
      return false;
    }

    bool InputState::OnMouseButtonPressedEvent(const Event::EventData& data)
    {
      ProcessButton(static_cast<MouseButton>(data.Data.u32[0]), true);
      return false;
    }

    bool InputState::OnMouseButtonReleasedEvent(const Event::EventData& data)
    {
      ProcessButton(static_cast<MouseButton>(data.Data.u32[0]), false);
      return false;
    }

    bool InputState::OnMouseMoveEvent(const Event::EventData& data)
    {
      ProcessMouseMove(data.Data.i32[0], data.Data.i32[1]);
      return false;
    }

    bool InputState::OnMouseWheelMoveEvent(const Event::EventData& data)
    {
      ProcessMouseWheel(data.Data.i32[0]);
      return false;
    }

    void InputState::ProcessKey(Key key, bool pressed)
    {
      CurrentKeyboard.Keys[static_cast<u32>(key)] = pressed;
    }

    void InputState::ProcessButton(MouseButton button, bool pressed)
    {
      CurrentMouse.Buttons[static_cast<u32>(button)] = pressed;
    }

    void InputState::ProcessMouseMove(i32 x, i32 y)
    {
      CurrentMouse.X = static_cast<i16>(x);
      CurrentMouse.Y = static_cast<i16>(y);
    }

    void InputState::ProcessMouseWheel(i32 zDelta)
    {
      CurrentMouse.WheelDelta = static_cast<i8>(zDelta);
    }

  }

} }
