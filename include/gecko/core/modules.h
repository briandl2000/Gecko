#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

namespace gecko {

class IModuleRegistry;

enum class ModuleResult : u8 {
  Ok = 0,
  InvalidArgument,
  DuplicateModule,
  NotFound,
  StartupFailed
};

struct IModule {
  virtual ~IModule() = default;

  [[nodiscard]] GECKO_API virtual Label RootLabel() const noexcept = 0;

  [[nodiscard]] GECKO_API virtual bool
  Startup(IModuleRegistry &modules) noexcept = 0;

  GECKO_API virtual void Shutdown(IModuleRegistry &modules) noexcept = 0;
};

class ModuleHandle {
public:
  ModuleHandle() = default;
  ModuleHandle(const ModuleHandle &) = delete;
  ModuleHandle &operator=(const ModuleHandle &) = delete;

  GECKO_API ModuleHandle(ModuleHandle &&other) noexcept;
  GECKO_API ModuleHandle &operator=(ModuleHandle &&other) noexcept;
  GECKO_API ~ModuleHandle() noexcept;

  [[nodiscard]] GECKO_API Label RootLabel() const noexcept { return m_label; }
  [[nodiscard]] GECKO_API explicit operator bool() const noexcept {
    return m_modules != nullptr && m_label.IsValid();
  }

  GECKO_API void Reset() noexcept;

  GECKO_API void Release() noexcept;

private:
  friend class IModuleRegistry;
  GECKO_API ModuleHandle(IModuleRegistry *modules, Label label) noexcept;

  IModuleRegistry *m_modules{nullptr};
  Label m_label{};
};

struct ModuleRegistration {
  ModuleHandle Handle;
  ModuleResult Result{ModuleResult::InvalidArgument};

  [[nodiscard]] bool Ok() const noexcept { return Result == ModuleResult::Ok; }
};

[[nodiscard]] GECKO_API ModuleResult InstallModule(IModule &module) noexcept;

class IModuleRegistry {
public:
  virtual ~IModuleRegistry() = default;

  [[nodiscard]] GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;

  [[nodiscard]] GECKO_API virtual ModuleRegistration
  RegisterStatic(IModule &module) noexcept = 0;

  [[nodiscard]] GECKO_API virtual ModuleResult
  Unregister(Label module) noexcept = 0;

  [[nodiscard]] GECKO_API virtual IModule *GetModule(Label module) noexcept = 0;
  [[nodiscard]] GECKO_API virtual const IModule *
  GetModule(Label module) const noexcept = 0;

  using ModuleVisitFn = void (*)(IModule &module, bool started,
                                 void *user) noexcept;
  GECKO_API virtual void ForEachModule(ModuleVisitFn fn,
                                       void *user) noexcept = 0;

  [[nodiscard]] GECKO_API virtual bool StartupAllModules() noexcept = 0;
  GECKO_API virtual void ShutdownAllModules() noexcept = 0;

protected:
  [[nodiscard]] ModuleHandle MakeHandle(Label label) noexcept {
    return ModuleHandle{this, label};
  }
};

} // namespace gecko
