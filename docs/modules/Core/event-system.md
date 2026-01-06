# Event System

## Overview

Gecko's event system provides broadcast-style notifications for decoupling modules and systems. It supports both immediate (synchronous) and delayed (queued) event dispatch.

## Key Characteristics

- **Notification-only**: No event consumption or ordering guarantees. Subscribers receive events in non-deterministic order.
- **Thread-safe enqueueing**: Events can be queued from any thread.
- **Single-threaded dispatch**: Events are dispatched on a designated owner thread (typically main thread).
- **Authorization model**: Only authorized modules can emit events for their domain via capability tokens.

## Event Codes

Event codes are 32-bit values with the following layout:
- **[31..24]**: Domain (module/system ID)
- **[23..0]**: Local event ID within that domain

Each module defines its own domain constant and event codes:

```cpp
namespace my_module_events {
constexpr u8 kMyModuleDomain = 0x42;
constexpr EventCode SomethingHappened = MakeEvent(kMyModuleDomain, 0x0001);
constexpr EventCode OtherEvent = MakeEvent(kMyModuleDomain, 0x0002);
}
```

## Publishing Events

### Delayed Publishing (Preferred)

Use `PublishEvent()` for general event publishing. The payload is copied into an internal queue and dispatched later when `DispatchQueuedEvents()` is called:

```cpp
EventEmitter emitter = GetEventBus()->CreateEmitter(kMyDomain);

MyPayload payload{};
payload.data = 42;

PublishEvent(emitter, MyEvent, payload);  // Queued for later dispatch
```

**When to use:**
- Default choice for most events
- Cross-thread event publishing
- When payload can be copied (no raw pointers to external data)

### Immediate Publishing (Use Sparingly)

Use `PublishImmediateEvent()` when you need synchronous notification. All callbacks are invoked on the calling thread before the function returns:

```cpp
SomeStackData data{};
PublishImmediateEvent(emitter, MyEvent, data);  // Callbacks execute here
```

**When to use:**
- Payload contains stack references that won't survive past the call
- Truly synchronous notification is required (rare)

## Subscribing to Events

Use `SubscribeEvent()` to register a callback. Returns an RAII handle that automatically unsubscribes when destroyed:

```cpp
class MySystem {
  EventSubscription m_Subscription;

public:
  void Init() {
    m_Subscription = SubscribeEvent(SomeEvent, &OnEventThunk, this);
  }

  static void OnEventThunk(void* user, const EventMeta& meta, EventView payload) {
    auto* self = static_cast<MySystem*>(user);
    auto* data = static_cast<const MyPayload*>(payload.data);
    // Handle event...
  }
};
```

**Subscription lifetime:**
- Keep the `EventSubscription` handle alive as long as you want to receive events
- Moving the handle transfers subscription ownership
- Destroying or resetting the handle unsubscribes automatically

## Event Loop Integration

Call `DispatchQueuedEvents()` early in your main loop, after platform event polling:

```cpp
void MainLoop() {
  while (running) {
    PollPlatformEvents();      // Platform → engine event translation
    DispatchQueuedEvents();    // Engine event dispatch
    UpdateGame();              // Game logic sees events from this frame
    Render();
  }
}
```

## Authorization Model

Events can only be published by authorized emitters. To emit events, a module must create an emitter for its domain:

```cpp
// In module initialization (e.g., window system):
EventEmitter m_WindowEmitter = GetEventBus()->CreateEmitter(kPlatformDomain);

// Later, when publishing:
PublishEvent(m_WindowEmitter, WindowResized, payload);
```

The emitter contains a capability token validated by the bus. Game code can subscribe to platform events but cannot forge a valid emitter to publish them.

## Threading Rules

- **Enqueue**: Safe to call from any thread. Payload is copied immediately.
- **Dispatch**: Must be called on the bus owner thread (usually main thread).
- **PublishImmediate**: Callbacks execute on the calling thread. Subscribers must be thread-safe if used cross-thread.
- **Subscribe/Unsubscribe**: Not thread-safe. Perform on the same thread that dispatches events.

## Design Constraints

The event system is intentionally simple:
- No event consumption (notifications only)
- No ordering guarantees between subscribers
- No propagation control

If you need input routing with consumption and ordered handling (e.g., UI consuming mouse clicks before gameplay), build a separate system. That system can emit notifications when input is consumed.

## Example: Platform Window Events

```cpp
// Platform module defines events (platform_events.h):
namespace gecko::platform_events {
constexpr u8 kPlatformDomain = 0x01;
constexpr EventCode WindowResized = MakeEvent(kPlatformDomain, 0x0001);

struct WindowResizedPayload {
  u32 width{0};
  u32 height{0};
};
}

// Platform system creates emitter and publishes:
class PlatformSystem {
  EventEmitter m_Emitter;

  void Init() {
    m_Emitter = GetEventBus()->CreateEmitter(platform_events::kPlatformDomain);
  }

  void OnWindowResize(u32 w, u32 h) {
    platform_events::WindowResizedPayload payload{w, h};
    PublishEvent(m_Emitter, platform_events::WindowResized, payload);
  }
};

// Game code subscribes:
class Renderer {
  EventSubscription m_ResizeSub;

  void Init() {
    m_ResizeSub = SubscribeEvent(
      platform_events::WindowResized,
      &OnResizeThunk,
      this
    );
  }

  static void OnResizeThunk(void* user, const EventMeta& meta, EventView payload) {
    auto* self = static_cast<Renderer*>(user);
    auto* p = static_cast<const platform_events::WindowResizedPayload*>(payload.data);
    self->OnResize(p->width, p->height);
  }

  void OnResize(u32 width, u32 height) {
    // Update viewport...
  }
};
```
