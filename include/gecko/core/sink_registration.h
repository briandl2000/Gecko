#pragma once

#include "gecko/core/api.h"

#include <concepts>

namespace gecko {

template <typename Service, typename SinkInterface>
concept SinkService = requires(Service& svc, SinkInterface* sink) {
  { svc.AddSink(sink) } noexcept;
  { svc.RemoveSink(sink) } noexcept;
};

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

  void Unregister() noexcept
  {
    if (m_Service)
    {
      m_Service->RemoveSink(static_cast<SinkInterface*>(this));
      m_Service = nullptr;
    }
  }

  [[nodiscard]] bool IsRegistered() const noexcept
  {
    return m_Service != nullptr;
  }

protected:
  Service* m_Service {nullptr};
};

}  // namespace gecko
