#pragma once

#include "gecko/gecko.h"

namespace gecko::examples::core_example {

class App
{
public:
  App() noexcept;
  ~App() noexcept;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Initialized;
  }

  int Run() noexcept;

private:
  bool m_Initialized {false};
};

}  // namespace gecko::examples::core_example
