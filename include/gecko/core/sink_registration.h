#pragma once

namespace gecko {

template <typename Service, typename Sink>
concept SinkService = requires(Service& service, Sink* sink) {
  service.AddSink(sink);
  service.RemoveSink(sink);
};

template <typename Sink, typename Service>
class RegisteredSink
{
public:
  virtual ~RegisteredSink() noexcept
  {
    Unregister();
  }

  RegisteredSink() noexcept = default;
  RegisteredSink(const RegisteredSink&) = delete;
  RegisteredSink& operator=(const RegisteredSink&) = delete;

  void RegisterWith(Service* service) noexcept
    requires SinkService<Service, Sink>
  {
    if (m_Service == service)
      return;
    Unregister();
    m_Service = service;
    if (m_Service != nullptr)
      m_Service->AddSink(static_cast<Sink*>(this));
  }

  void Unregister() noexcept
  {
    if (m_Service != nullptr)
    {
      m_Service->RemoveSink(static_cast<Sink*>(this));
      m_Service = nullptr;
    }
  }

private:
  Service* m_Service {nullptr};
};

}  // namespace gecko
