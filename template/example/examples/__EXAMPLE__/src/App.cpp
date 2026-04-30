#include "App.h"

#include <gecko/core/labels.h>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>

namespace app::__EXAMPLE__ {

namespace labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.__EXAMPLE__");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.__EXAMPLE__.main");
}  // namespace labels

::gecko::Label App::AppModule::RootLabel() const noexcept
{
  return labels::App;
}

bool App::AppModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::AppModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App()
{
  if (!m_AllocScope)
    return;

  m_Engine = ::gecko::Engine::Create({&m_RuntimeModule, &m_AppModule});
  if (!m_Engine)
    return;

  m_LogSinks.emplace();
}

int App::Run()
{
  if (!IsValid())
    return 1;
  GECKO_INFO(labels::Main, "Hello from __EXAMPLE__!");
  return 0;
}

}  // namespace app::__EXAMPLE__
