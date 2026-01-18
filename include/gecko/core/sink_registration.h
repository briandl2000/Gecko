#pragma once

#include "gecko/core/api.h"

#include <concepts>

namespace gecko {

// Concept to verify that a Service has the required AddSinkImpl/RemoveSinkImpl
// methods for a given SinkInterface
template <typename Service, typename SinkInterface>
concept SinkService = requires(Service& svc, SinkInterface* sink) {
  { svc.AddSinkImpl(sink) } noexcept;
  { svc.RemoveSinkImpl(sink) } noexcept;
};

// Base class for sinks that auto-register/unregister with a service.
// Inherit from this in your sink interface (e.g., ILogSink, IProfilerSink).
//
// Usage:
//   struct ILogSink : RegisteredSink<ILogSink, ILogger> { ... };
//   class ConsoleLogSink : public ILogSink { ... };
//
//   ConsoleLogSink sink;
//   sink.RegisterWith(GetLogger());  // Auto-unregisters when sink is destroyed
//
template <typename SinkInterface, typename Service>
class RegisteredSink
{
public:
  RegisteredSink() noexcept = default;

  virtual ~RegisteredSink() noexcept
  {
    Unregister();
  }

  // Non-copyable, non-movable (registration is tied to this object's address)
  RegisteredSink(const RegisteredSink&) = delete;
  RegisteredSink& operator=(const RegisteredSink&) = delete;
  RegisteredSink(RegisteredSink&&) = delete;
  RegisteredSink& operator=(RegisteredSink&&) = delete;

  // Register this sink with a service. Can only be registered with one service
  // at a time.
  void RegisterWith(Service* service) noexcept
    requires SinkService<Service, SinkInterface>
  {
    // If already registered with a different service, unregister first
    if (m_Service && m_Service != service)
    {
      Unregister();
    }

    m_Service = service;
    if (m_Service)
    {
      m_Service->AddSinkImpl(static_cast<SinkInterface*>(this));
    }
  }

  // Manually unregister from the service
  void Unregister() noexcept
  {
    if (m_Service)
    {
      m_Service->RemoveSinkImpl(static_cast<SinkInterface*>(this));
      m_Service = nullptr;
    }
  }

  // Check if currently registered
  [[nodiscard]] bool IsRegistered() const noexcept
  {
    return m_Service != nullptr;
  }

protected:
  Service* m_Service {nullptr};
};

}  // namespace gecko
