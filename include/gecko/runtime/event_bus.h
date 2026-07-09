#pragma once

/// @file
/// `EventBus` -- reference `IEventBus` implementation.
///
/// Supports both immediate and queued subscriber delivery, plus
/// per-module emitter capability validation.

#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"

namespace gecko::runtime {

/// Reference event-bus implementation. Routes `Send()` calls to
/// matching subscribers either immediately on the caller's thread or
/// queued for `Dispatch()`.
class EventBus final : public IEventBus
{
public:
  EventBus();
  ~EventBus() override;

  EventSubscription Subscribe(EventCode code, CallbackFn fn, void* user,
                              SubscriptionOptions options = {}) noexcept override;
  void Send(const EventEmitter& emitter, EventCode code, EventView payload) noexcept override;
  usize Dispatch(usize maxCount) noexcept override;

  bool RegisterModule(u64 moduleId) noexcept override;
  void UnregisterModule(u64 moduleId) noexcept override;
  EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept override;
  bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) const noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

protected:
  void Unsubscribe(u64 id) noexcept override;

private:
  struct Impl;

  void NotifySubscribers(EventCode code, const EventMeta& meta, EventView payload, SubscriptionDelivery deliveryFilter);

  ::gecko::Unique<Impl> m_Impl;
};

}  // namespace gecko::runtime
