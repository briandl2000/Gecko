#include "gecko/runtime/module_registry.h"

#include <unordered_map>
#include <vector>

#include "gecko/core/assert.h"
#include "gecko/core/log.h"
#include "gecko/core/services.h"
#include "labels.h"

namespace gecko::runtime {

struct ModuleRegistry::Impl {
  struct ModuleRecord {
    ::gecko::Label Root{};
    ::gecko::IModule *Module{nullptr};
    bool Started{false};
  };

  static void EraseFirstU64(std::vector<u64> &v, u64 value) noexcept {
    for (auto it = v.begin(); it != v.end(); ++it) {
      if (*it == value) {
        v.erase(it);
        return;
      }
    }
  }

  [[nodiscard]] ModuleRecord *FindModuleRecord(::gecko::Label root) noexcept {
    auto it = Modules.find(root.Id);
    return (it != Modules.end()) ? &it->second : nullptr;
  }

  [[nodiscard]] const ModuleRecord *
  FindModuleRecord(::gecko::Label root) const noexcept {
    auto it = Modules.find(root.Id);
    return (it != Modules.end()) ? &it->second : nullptr;
  }

  [[nodiscard]] bool StartupModule(ModuleRegistry &self,
                                   ModuleRecord &rec) noexcept {
    if (rec.Started) {
      return true;
    }
    GECKO_ASSERT(rec.Module != nullptr);
    if (!rec.Module->Startup(self)) {
      return false;
    }
    rec.Started = true;
    return true;
  }

  void ShutdownModule(ModuleRegistry &self, ModuleRecord &rec) noexcept {
    if (!rec.Started) {
      return;
    }
    GECKO_ASSERT(rec.Module != nullptr);
    rec.Module->Shutdown(self);
    rec.Started = false;
  }

  bool Booted{false};
  std::unordered_map<u64, ModuleRecord> Modules;
  std::vector<u64> RegistrationOrder;
};

void ModuleRegistry::ImplDeleter::operator()(Impl *ptr) const noexcept {
  delete ptr;
}

ModuleRegistry::~ModuleRegistry() = default;

bool ModuleRegistry::Init() noexcept {
  if (!m_impl) {
    m_impl.reset(new Impl());
  }
  // Service init corresponds to "engine booted": newly registered modules
  // should be started immediately.
  m_impl->Booted = true;
  return true;
}

void ModuleRegistry::Shutdown() noexcept {
  if (!m_impl) {
    return;
  }
  ShutdownAllModules();
  m_impl->Modules.clear();
  m_impl->RegistrationOrder.clear();
  m_impl->Booted = false;
}

::gecko::ModuleRegistration
ModuleRegistry::RegisterStatic(::gecko::IModule &module) noexcept {
  GECKO_PROF_FUNC(labels::Modules);
  
  if (!m_impl) {
    m_impl.reset(new Impl());
  }

  const ::gecko::Label root = module.RootLabel();
  GECKO_INFO(labels::Modules, "RegisterStatic: %s",
             root.Name ? root.Name : "(unnamed)");

  if (!root.IsValid()) {
    GECKO_WARN(labels::Modules, "RegisterStatic failed: invalid root label");
    return ::gecko::ModuleRegistration{::gecko::ModuleHandle{},
                                       ::gecko::ModuleResult::InvalidArgument};
  }

  if (m_impl->Modules.contains(root.Id)) {
    GECKO_WARN(labels::Modules, "RegisterStatic failed: duplicate module %s",
               root.Name ? root.Name : "(unnamed)");
    return ::gecko::ModuleRegistration{::gecko::ModuleHandle{},
                                       ::gecko::ModuleResult::DuplicateModule};
  }

  // Register module ID with event bus for event scoping
  auto *eventBus = ::gecko::GetEventBus();
  if (eventBus && !eventBus->RegisterModule(root.Id)) {
    GECKO_WARN(labels::Modules,
               "RegisterStatic failed: module ID %llu already registered with "
               "event bus for %s",
               static_cast<unsigned long long>(root.Id),
               root.Name ? root.Name : "(unnamed)");
    return ::gecko::ModuleRegistration{::gecko::ModuleHandle{},
                                       ::gecko::ModuleResult::DuplicateModule};
  }

  Impl::ModuleRecord rec;
  rec.Root = root;
  rec.Module = &module;
  rec.Started = false;

  m_impl->Modules.emplace(root.Id, std::move(rec));
  m_impl->RegistrationOrder.push_back(root.Id);

  if (m_impl->Booted) {
    auto *inserted = m_impl->FindModuleRecord(root);
    if (!inserted) {
      return ::gecko::ModuleRegistration{
          ::gecko::ModuleHandle{}, ::gecko::ModuleResult::InvalidArgument};
    }
    if (!m_impl->StartupModule(*this, *inserted)) {
      (void)Unregister(root);
      GECKO_ERROR(labels::Modules, "Startup failed for %s",
                  root.Name ? root.Name : "(unnamed)");
      return ::gecko::ModuleRegistration{::gecko::ModuleHandle{},
                                         ::gecko::ModuleResult::StartupFailed};
    }
  }

  GECKO_INFO(labels::Modules, "Registered: %s",
             root.Name ? root.Name : "(unnamed)");
  return ::gecko::ModuleRegistration{MakeHandle(root),
                                     ::gecko::ModuleResult::Ok};
}

::gecko::ModuleResult
ModuleRegistry::Unregister(::gecko::Label module) noexcept {
  if (!m_impl) {
    return ::gecko::ModuleResult::NotFound;
  }

  GECKO_INFO(labels::Modules, "Unregister: %s",
             module.Name ? module.Name : "(unnamed)");
  auto it = m_impl->Modules.find(module.Id);
  if (it == m_impl->Modules.end()) {
    GECKO_WARN(labels::Modules, "Unregister failed: not found %s",
               module.Name ? module.Name : "(unnamed)");
    return ::gecko::ModuleResult::NotFound;
  }

  Impl::ModuleRecord &rec = it->second;

  m_impl->ShutdownModule(*this, rec);

  // Unregister from event bus
  auto *eventBus = ::gecko::GetEventBus();
  if (eventBus) {
    eventBus->UnregisterModule(module.Id);
  }

  Impl::EraseFirstU64(m_impl->RegistrationOrder, module.Id);
  m_impl->Modules.erase(it);
  GECKO_INFO(labels::Modules, "Unregistered: %s",
             module.Name ? module.Name : "(unnamed)");
  return ::gecko::ModuleResult::Ok;
}

::gecko::IModule *ModuleRegistry::GetModule(::gecko::Label module) noexcept {
  if (!m_impl) {
    return nullptr;
  }
  auto *rec = m_impl->FindModuleRecord(module);
  return rec ? rec->Module : nullptr;
}

const ::gecko::IModule *
ModuleRegistry::GetModule(::gecko::Label module) const noexcept {
  if (!m_impl) {
    return nullptr;
  }
  auto *rec = m_impl->FindModuleRecord(module);
  return rec ? rec->Module : nullptr;
}

void ModuleRegistry::ForEachModule(ModuleVisitFn fn, void *user) noexcept {
  if (!fn || !m_impl) {
    return;
  }
  for (u64 id : m_impl->RegistrationOrder) {
    auto it = m_impl->Modules.find(id);
    if (it == m_impl->Modules.end() || it->second.Module == nullptr) {
      continue;
    }
    fn(*it->second.Module, it->second.Started, user);
  }
}

bool ModuleRegistry::StartupAllModules() noexcept {
  if (!m_impl) {
    m_impl.reset(new Impl());
  }
  m_impl->Booted = true;

  std::vector<u64> startedThisCall;
  startedThisCall.reserve(m_impl->RegistrationOrder.size());

  for (u64 id : m_impl->RegistrationOrder) {
    auto it = m_impl->Modules.find(id);
    if (it == m_impl->Modules.end()) {
      continue;
    }
    if (it->second.Started) {
      continue;
    }

    if (!m_impl->StartupModule(*this, it->second)) {
      for (auto rit = startedThisCall.rbegin(); rit != startedThisCall.rend();
           ++rit) {
        auto it2 = m_impl->Modules.find(*rit);
        if (it2 != m_impl->Modules.end()) {
          m_impl->ShutdownModule(*this, it2->second);
        }
      }
      return false;
    }

    startedThisCall.push_back(id);
  }

  return true;
}

void ModuleRegistry::ShutdownAllModules() noexcept {
  if (!m_impl) {
    return;
  }

  if (m_impl->RegistrationOrder.empty()) {
    m_impl->Booted = false;
    return;
  }

  GECKO_INFO(labels::Modules, "ShutdownAllModules (booted=%s)",
             m_impl->Booted ? "true" : "false");

  for (auto rit = m_impl->RegistrationOrder.rbegin();
       rit != m_impl->RegistrationOrder.rend(); ++rit) {
    auto it = m_impl->Modules.find(*rit);
    if (it != m_impl->Modules.end()) {
      if (it->second.Started) {
        GECKO_INFO(labels::Modules, "Shutdown: %s",
                   it->second.Root.Name ? it->second.Root.Name : "(unnamed)");
        m_impl->ShutdownModule(*this, it->second);
      }
    }
  }

  m_impl->Booted = false;
}
} // namespace gecko::runtime
