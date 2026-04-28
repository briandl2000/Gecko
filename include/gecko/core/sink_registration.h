#pragma once

/// @file
/// CRTP-style helper for self-registering sinks (logger, profiler, ...).
///
/// Sinks derive from `RegisteredSink<SinkInterface, Service>` and call
/// `RegisterWith(service)` to attach themselves. The destructor
/// guarantees an `Unregister()` call so a sink that goes out of scope
/// can never leave a dangling pointer in the service.

#include "gecko/core/api.h"

#include <concepts>

namespace gecko {

/// Concept satisfied by any service that exposes the
/// `AddSink`/`RemoveSink` pair `RegisteredSink` needs.
template <typename Service, typename SinkInterface>
concept SinkService = requires(Service& svc, SinkInterface* sink) {
  { svc.AddSink(sink) } noexcept;
  { svc.RemoveSink(sink) } noexcept;
};

/// Base class for sinks that own their own registration with a service.
///
/// Non-copyable and non-movable: registration is keyed on the sink's
/// address, so moving would silently break the link. Construct in
/// place, call `RegisterWith()` once the service exists, and let RAII
/// detach the sink at destruction.
///
/// @tparam SinkInterface  The interface the service expects (e.g.
///                        `ILogSink`). The derived class must publicly
///                        inherit from it.
/// @tparam Service        The service exposing `AddSink`/`RemoveSink`.
template <typename SinkInterface, typename Service>
class RegisteredSink
{
public:
  RegisteredSink() noexcept = default;

  virtual ~RegisteredSink() noexcept
  {
    Unregister();
  }

  RegisteredSink(const RegisteredSink&) = delete;
  RegisteredSink& operator=(const RegisteredSink&) = delete;
  RegisteredSink(RegisteredSink&&) = delete;
  RegisteredSink& operator=(RegisteredSink&&) = delete;

  /// Attach this sink to a service.
  ///
  /// If already registered with a different service, unregisters
  /// first. Calling with the current owner is a no-op.
  /// @param service Service to register with. Pass null to unregister.
  void RegisterWith(Service* service) noexcept
    requires SinkService<Service, SinkInterface>
  {
    if (m_Service && m_Service != service)
    {
      Unregister();
    }

    m_Service = service;
    if (m_Service)
    {
      m_Service->AddSink(static_cast<SinkInterface*>(this));
    }
  }

  /// Detach from the service. Safe to call when not registered.
  void Unregister() noexcept
  {
    if (m_Service)
    {
      m_Service->RemoveSink(static_cast<SinkInterface*>(this));
      m_Service = nullptr;
    }
  }

  /// @return `true` while attached to a service.
  [[nodiscard]] bool IsRegistered() const noexcept
  {
    return m_Service != nullptr;
  }

protected:
  Service* m_Service {nullptr};
};

}  // namespace gecko
