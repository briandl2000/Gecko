#pragma once

#include "gecko/core/utility/move.h"

namespace gecko {

template <typename Function>
class DeferredCall
{
public:
  explicit DeferredCall(Function function) noexcept : m_Function(Move(function))
  {}

  ~DeferredCall() noexcept
  {
    if (m_Active)
      m_Function();
  }

  DeferredCall(const DeferredCall&) = delete;
  DeferredCall& operator=(const DeferredCall&) = delete;

  DeferredCall(DeferredCall&& other) noexcept : m_Function(Move(other.m_Function)), m_Active(other.m_Active)
  {
    other.m_Active = false;
  }

  void Cancel() noexcept
  {
    m_Active = false;
  }

private:
  Function m_Function;
  bool m_Active {true};
};

template <typename Function>
[[nodiscard]] DeferredCall<RemoveCVRef<Function>> Defer(Function&& function) noexcept
{
  return DeferredCall<RemoveCVRef<Function>>(Forward<Function>(function));
}

}  // namespace gecko
