#include "gecko/core/modules.h" // Corresponding header first

#include "gecko/core/services.h"

namespace gecko {

ModuleResult InstallModule(IModule &module) noexcept {
  ModuleRegistration reg = GetModules()->RegisterStatic(module);
  if (reg.Ok()) {
    reg.Handle.Release();
  }
  return reg.Result;
}

ModuleHandle::ModuleHandle(IModuleRegistry *modules, Label label) noexcept
    : m_modules{modules}, m_label{label} {}

ModuleHandle::ModuleHandle(ModuleHandle &&other) noexcept {
  m_modules = other.m_modules;
  m_label = other.m_label;
  other.m_modules = nullptr;
  other.m_label = {};
}

ModuleHandle &ModuleHandle::operator=(ModuleHandle &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  Reset();

  m_modules = other.m_modules;
  m_label = other.m_label;
  other.m_modules = nullptr;
  other.m_label = {};

  return *this;
}

ModuleHandle::~ModuleHandle() noexcept { Reset(); }

void ModuleHandle::Reset() noexcept {
  if (m_modules && m_label.IsValid()) {
    (void)m_modules->Unregister(m_label);
  }

  m_modules = nullptr;
  m_label = {};
}

void ModuleHandle::Release() noexcept {
  m_modules = nullptr;
  m_label = {};
}

} // namespace gecko
