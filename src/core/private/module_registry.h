#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/modules.h"

#include <memory>

namespace gecko::core::detail {

// Concrete IModuleRegistry implementation owned by Engine and constructed
// inside the CoreServices shared library so the registry's storage lives
// alongside the global service singletons it manages. Not part of the
// public API: clients interact via IModuleRegistry only.
class ModuleRegistry final : public ::gecko::IModuleRegistry
{
public:
  ModuleRegistry() = default;
  ModuleRegistry(const ModuleRegistry&) = delete;
  ModuleRegistry& operator=(const ModuleRegistry&) = delete;
  ~ModuleRegistry() override;

  [[nodiscard]] bool Init() noexcept override;
  void Shutdown() noexcept override;

  [[nodiscard]] ::gecko::ModuleRegistration RegisterStatic(::gecko::IModule& module) noexcept override;

  [[nodiscard]] ::gecko::ModuleResult Unregister(::gecko::Label module) noexcept override;

  [[nodiscard]] ::gecko::IModule* GetModule(::gecko::Label module) noexcept override;

  [[nodiscard]] const ::gecko::IModule* GetModule(::gecko::Label module) const noexcept override;

  void ForEachModule(ModuleVisitFn fn, void* user) noexcept override;

  [[nodiscard]] bool StartupAllModules() noexcept override;
  void ShutdownAllModules() noexcept override;

  [[nodiscard]] bool PublishServiceImpl(::gecko::ServiceId id, void* impl) noexcept override;
  [[nodiscard]] void* GetServiceImpl(::gecko::ServiceId id) const noexcept override;
  [[nodiscard]] bool UnpublishServiceImpl(::gecko::ServiceId id) noexcept override;

private:
  struct Impl;
  struct ImplDeleter
  {
    void operator()(Impl* ptr) const noexcept;
  };
  ::std::unique_ptr<Impl, ImplDeleter> m_impl;
};

}  // namespace gecko::core::detail
