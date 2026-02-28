#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/modules.h"

#include <memory>

namespace gecko::runtime {

// Concrete module registry implementation (service) provided by Runtime.
class ModuleRegistry final : public ::gecko::IModuleRegistry
{
public:
  GECKO_API ModuleRegistry() = default;
  ModuleRegistry(const ModuleRegistry&) = delete;
  ModuleRegistry& operator=(const ModuleRegistry&) = delete;
  GECKO_API ~ModuleRegistry() override;

  [[nodiscard]] GECKO_API bool Init() noexcept override;
  GECKO_API void Shutdown() noexcept override;

  [[nodiscard]] GECKO_API ::gecko::ModuleRegistration RegisterStatic(
      ::gecko::IModule& module) noexcept override;

  [[nodiscard]] GECKO_API ::gecko::ModuleResult Unregister(
      ::gecko::Label module) noexcept override;

  [[nodiscard]] GECKO_API ::gecko::IModule* GetModule(
      ::gecko::Label module) noexcept override;

  [[nodiscard]] GECKO_API const ::gecko::IModule* GetModule(
      ::gecko::Label module) const noexcept override;

  GECKO_API void ForEachModule(ModuleVisitFn fn, void* user) noexcept override;

  [[nodiscard]] GECKO_API bool StartupAllModules() noexcept override;
  GECKO_API void ShutdownAllModules() noexcept override;

private:
  struct Impl;
  struct ImplDeleter
  {
    GECKO_API void operator()(Impl* ptr) const noexcept;
  };
  std::unique_ptr<Impl, ImplDeleter> m_impl;
};

}  // namespace gecko::runtime
