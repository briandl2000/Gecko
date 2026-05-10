#include "private/module_registry.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/hash.h"
#include "private/labels.h"

#include <unordered_map>
#include <vector>

namespace gecko::core::detail {

struct ModuleRegistry::Impl
{
  struct ModuleRecord
  {
    ::gecko::Label Root {};
    ::gecko::IModule* Module {nullptr};
    bool Started {false};
  };

  struct ServiceEntry
  {
    ::gecko::ServiceId Id {};
    void* Impl {nullptr};
  };

  static void EraseFirstU64(std::vector<u64>& v, u64 value) noexcept
  {
    for (auto it = v.begin(); it != v.end(); ++it)
    {
      if (*it == value)
      {
        v.erase(it);
        return;
      }
    }
  }

  [[nodiscard]] ModuleRecord* FindModuleRecord(::gecko::Label root) noexcept
  {
    auto it = Modules.find(root.Id);
    return (it != Modules.end()) ? &it->second : nullptr;
  }

  [[nodiscard]] const ModuleRecord* FindModuleRecord(::gecko::Label root) const noexcept
  {
    auto it = Modules.find(root.Id);
    return (it != Modules.end()) ? &it->second : nullptr;
  }

  [[nodiscard]] bool StartupModule(ModuleRegistry& self, ModuleRecord& rec) noexcept
  {
    if (rec.Started)
    {
      return true;
    }
    GECKO_ASSERT(rec.Module != nullptr);
    const char* name = rec.Root.Name ? rec.Root.Name : "Module::Startup";
    ::gecko::ProfScope _scope_module_startup(::gecko::core::labels::Modules, ::gecko::FNV1a(name), name,
                                             ::gecko::ProfLevel::Normal);
    if (!rec.Module->Startup(self))
    {
      return false;
    }
    rec.Started = true;
    StartupOrder.push_back(rec.Root.Id);
    return true;
  }

  void ShutdownModule(ModuleRegistry& self, ModuleRecord& rec) noexcept
  {
    if (!rec.Started)
    {
      return;
    }
    GECKO_ASSERT(rec.Module != nullptr);
    rec.Module->Shutdown(self);
    rec.Started = false;
    // Drop from StartupOrder so any later batch can re-walk a clean
    // record. Linear scan is fine: this list is short and shutdown is
    // not on a hot path.
    for (auto it = StartupOrder.begin(); it != StartupOrder.end(); ++it)
    {
      if (*it == rec.Root.Id)
      {
        StartupOrder.erase(it);
        break;
      }
    }
  }

  [[nodiscard]] void* FindServiceImpl(::gecko::ServiceId id) const noexcept
  {
    for (const auto& s : Services)
    {
      if (s.Id == id)
      {
        return s.Impl;
      }
    }
    return nullptr;
  }

  bool Booted {false};
  std::unordered_map<u64, ModuleRecord> Modules;
  std::vector<u64> RegistrationOrder;
  // Order in which modules actually completed Startup successfully.
  // Shutdown iterates this in reverse, so dependents are torn down
  // before the modules they depend on. This may differ from
  // RegistrationOrder when StartupAllModules() topologically reorders
  // a batch.
  std::vector<u64> StartupOrder;
  std::vector<ServiceEntry> Services;
};

void ModuleRegistry::ImplDeleter::operator()(Impl* ptr) const noexcept
{
  delete ptr;
}

ModuleRegistry::~ModuleRegistry() = default;

bool ModuleRegistry::Init() noexcept
{
  if (!m_impl)
  {
    try
    {
      m_impl.reset(new Impl());
    }
    catch (const std::exception&)
    {
      return false;
    }
    catch (...)
    {
      return false;
    }
  }
  // Init() prepares storage but does NOT mark the registry as booted.
  // StartupAllModules() is responsible for setting Booted=true after it
  // has computed the topological start order. This lets Engine::Create
  // register every module first and then start them in dependency order.
  // Modules registered AFTER StartupAllModules() will auto-start because
  // Booted will be true at that point.
  return true;
}

void ModuleRegistry::Shutdown() noexcept
{
  if (!m_impl)
  {
    return;
  }
  ShutdownAllModules();
  m_impl->Modules.clear();
  m_impl->RegistrationOrder.clear();
  m_impl->Services.clear();
  m_impl->Booted = false;

  // Deallocate Impl before allocator is uninstalled
  m_impl.reset();
}

::gecko::ModuleRegistration ModuleRegistry::RegisterStatic(::gecko::IModule& module) noexcept
{
  GECKO_SCOPE(::gecko::core::labels::Modules);

  if (!m_impl)
  {
    m_impl.reset(new (::std::nothrow) Impl());
    if (!m_impl)
    {
      return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::OutOfMemory};
    }
  }

  const ::gecko::Label root = module.RootLabel();
  GECKO_INFO(::gecko::core::labels::Modules, "RegisterStatic: %s", root.Name ? root.Name : "(unnamed)");

  if (!root.IsValid())
  {
    GECKO_WARN(::gecko::core::labels::Modules, "RegisterStatic failed: invalid root label");
    return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::InvalidArgument};
  }

  if (m_impl->Modules.contains(root.Id))
  {
    GECKO_WARN(::gecko::core::labels::Modules, "RegisterStatic failed: duplicate module %s",
               root.Name ? root.Name : "(unnamed)");
    return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::DuplicateModule};
  }

  // Register module ID with event bus for event scoping
  auto* eventBus = ::gecko::GetEventBus();
  if (eventBus && !eventBus->RegisterModule(root.Id))
  {
    GECKO_WARN(::gecko::core::labels::Modules,
               "RegisterStatic failed: module ID %llu already registered with "
               "event bus for %s",
               static_cast<unsigned long long>(root.Id), root.Name ? root.Name : "(unnamed)");
    return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::DuplicateModule};
  }

  Impl::ModuleRecord rec;
  rec.Root = root;
  rec.Module = &module;
  rec.Started = false;

  m_impl->Modules.emplace(root.Id, std::move(rec));
  m_impl->RegistrationOrder.push_back(root.Id);

  if (m_impl->Booted)
  {
    auto* inserted = m_impl->FindModuleRecord(root);
    if (!inserted)
    {
      return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::InvalidArgument};
    }
    if (!m_impl->StartupModule(*this, *inserted))
    {
      (void)Unregister(root);
      GECKO_ERROR(::gecko::core::labels::Modules, "Startup failed for %s", root.Name ? root.Name : "(unnamed)");
      return ::gecko::ModuleRegistration {::gecko::ModuleHandle {}, ::gecko::ModuleResult::StartupFailed};
    }
  }

  GECKO_INFO(::gecko::core::labels::Modules, "Registered: %s", root.Name ? root.Name : "(unnamed)");
  return ::gecko::ModuleRegistration {MakeHandle(root), ::gecko::ModuleResult::Ok};
}

::gecko::ModuleResult ModuleRegistry::Unregister(::gecko::Label module) noexcept
{
  if (!m_impl)
  {
    return ::gecko::ModuleResult::NotFound;
  }

  GECKO_INFO(::gecko::core::labels::Modules, "Unregister: %s", module.Name ? module.Name : "(unnamed)");
  auto it = m_impl->Modules.find(module.Id);
  if (it == m_impl->Modules.end())
  {
    GECKO_WARN(::gecko::core::labels::Modules, "Unregister failed: not found %s",
               module.Name ? module.Name : "(unnamed)");
    return ::gecko::ModuleResult::NotFound;
  }

  Impl::ModuleRecord& rec = it->second;

  m_impl->ShutdownModule(*this, rec);

  // Unregister from event bus
  auto* eventBus = ::gecko::GetEventBus();
  if (eventBus)
  {
    eventBus->UnregisterModule(module.Id);
  }

  Impl::EraseFirstU64(m_impl->RegistrationOrder, module.Id);
  m_impl->Modules.erase(it);
  GECKO_INFO(::gecko::core::labels::Modules, "Unregistered: %s", module.Name ? module.Name : "(unnamed)");
  return ::gecko::ModuleResult::Ok;
}

::gecko::IModule* ModuleRegistry::GetModule(::gecko::Label module) noexcept
{
  if (!m_impl)
  {
    return nullptr;
  }
  auto* rec = m_impl->FindModuleRecord(module);
  return rec ? rec->Module : nullptr;
}

const ::gecko::IModule* ModuleRegistry::GetModule(::gecko::Label module) const noexcept
{
  if (!m_impl)
  {
    return nullptr;
  }
  auto* rec = m_impl->FindModuleRecord(module);
  return rec ? rec->Module : nullptr;
}

void ModuleRegistry::ForEachModule(ModuleVisitFn fn, void* user) noexcept
{
  if (!fn || !m_impl)
  {
    return;
  }
  for (u64 id : m_impl->RegistrationOrder)
  {
    auto it = m_impl->Modules.find(id);
    if (it == m_impl->Modules.end() || it->second.Module == nullptr)
    {
      continue;
    }
    fn(*it->second.Module, it->second.Started, user);
  }
}

bool ModuleRegistry::StartupAllModules() noexcept
{
  if (!m_impl)
  {
    m_impl.reset(new (::std::nothrow) Impl());
    if (!m_impl)
    {
      return false;
    }
  }
  m_impl->Booted = true;

  // Build a list of (id, module*) for unstarted modules in registration
  // order. Already-started modules are skipped (they were registered
  // after a previous StartupAllModules).
  struct Node
  {
    u64 Id {0};
    ::gecko::IModule* Module {nullptr};
    int InDegree {0};
  };
  std::vector<Node> nodes;
  nodes.reserve(m_impl->RegistrationOrder.size());
  for (u64 id : m_impl->RegistrationOrder)
  {
    auto it = m_impl->Modules.find(id);
    if (it == m_impl->Modules.end() || it->second.Started)
    {
      continue;
    }
    nodes.push_back(Node {id, it->second.Module, 0});
  }

  // publisherOf: ServiceId -> index into `nodes`. Diagnose duplicate
  // publishers as an error.
  struct PublisherEntry
  {
    u64 ServiceIdValue {0};
    int NodeIndex {-1};
  };
  std::vector<PublisherEntry> publishers;
  publishers.reserve(nodes.size() * 2);
  auto findPublisher = [&](u64 sid) -> int {
    for (const auto& p : publishers)
    {
      if (p.ServiceIdValue == sid)
      {
        return p.NodeIndex;
      }
    }
    return -1;
  };

  for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
  {
    auto pubs = nodes[i].Module->Publishes();
    for (const auto& pid : pubs)
    {
      int existing = findPublisher(pid.Value);
      if (existing >= 0)
      {
        GECKO_ERROR(::gecko::core::labels::Modules,
                    "Duplicate service publisher: '%s' and '%s' both publish "
                    "service id 0x%016llx",
                    nodes[existing].Module->RootLabel().Name ? nodes[existing].Module->RootLabel().Name : "(unnamed)",
                    nodes[i].Module->RootLabel().Name ? nodes[i].Module->RootLabel().Name : "(unnamed)",
                    static_cast<unsigned long long>(pid.Value));
        return false;
      }
      publishers.push_back(PublisherEntry {pid.Value, i});
    }
  }

  // Build adjacency: edges[from] = list of `to`-indices (i.e. modules
  // that depend on `from`). Compute in-degrees.
  std::vector<std::vector<int>> edges(nodes.size());
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
  {
    auto reqs = nodes[i].Module->Requires();
    for (const auto& rid : reqs)
    {
      // Service may already be published by a prior, already-started
      // module (registered earlier and started in a previous
      // StartupAllModules invocation). In that case it's satisfied and
      // contributes no edge.
      if (m_impl->FindServiceImpl(rid) != nullptr && findPublisher(rid.Value) < 0)
      {
        continue;
      }

      int producer = findPublisher(rid.Value);
      if (producer < 0)
      {
        GECKO_ERROR(::gecko::core::labels::Modules,
                    "Module '%s' requires service id 0x%016llx but no "
                    "registered module publishes it",
                    nodes[i].Module->RootLabel().Name ? nodes[i].Module->RootLabel().Name : "(unnamed)",
                    static_cast<unsigned long long>(rid.Value));
        return false;
      }
      if (producer == i)
      {
        // self-publishes a self-required service: no edge.
        continue;
      }
      edges[producer].push_back(i);
      ++nodes[i].InDegree;
    }
  }

  // Kahn's algorithm.
  std::vector<int> ready;
  ready.reserve(nodes.size());
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
  {
    if (nodes[i].InDegree == 0)
    {
      ready.push_back(i);
    }
  }

  std::vector<int> order;
  order.reserve(nodes.size());
  while (!ready.empty())
  {
    int n = ready.back();
    ready.pop_back();
    order.push_back(n);
    for (int m : edges[n])
    {
      if (--nodes[m].InDegree == 0)
      {
        ready.push_back(m);
      }
    }
  }

  if (order.size() != nodes.size())
  {
    GECKO_ERROR(::gecko::core::labels::Modules,
                "Module dependency cycle detected (%zu of %zu modules in "
                "topological order)",
                order.size(), nodes.size());
    return false;
  }

  // Start in the computed order; rollback on failure.
  std::vector<u64> startedThisCall;
  startedThisCall.reserve(order.size());
  for (int idx : order)
  {
    auto it = m_impl->Modules.find(nodes[idx].Id);
    if (it == m_impl->Modules.end())
    {
      continue;
    }
    if (!m_impl->StartupModule(*this, it->second))
    {
      for (auto rit = startedThisCall.rbegin(); rit != startedThisCall.rend(); ++rit)
      {
        auto it2 = m_impl->Modules.find(*rit);
        if (it2 != m_impl->Modules.end())
        {
          m_impl->ShutdownModule(*this, it2->second);
        }
      }
      return false;
    }
    startedThisCall.push_back(nodes[idx].Id);
  }

  return true;
}

void ModuleRegistry::ShutdownAllModules() noexcept
{
  if (!m_impl)
  {
    return;
  }

  if (m_impl->RegistrationOrder.empty())
  {
    m_impl->Booted = false;
    return;
  }

  GECKO_INFO(::gecko::core::labels::Modules, "ShutdownAllModules (booted=%s)", m_impl->Booted ? "true" : "false");

  // Shutdown in REVERSE order of successful Startup completion. This is
  // the inverse of the topological order computed by StartupAllModules
  // (or the registration order for late-auto-started modules), so a
  // module is always torn down before any module it transitively
  // depended on. ShutdownModule erases from StartupOrder, so we copy
  // the ids out first to avoid iterator invalidation.
  std::vector<u64> shutdownOrder(m_impl->StartupOrder.rbegin(), m_impl->StartupOrder.rend());
  for (u64 id : shutdownOrder)
  {
    auto it = m_impl->Modules.find(id);
    if (it != m_impl->Modules.end() && it->second.Started)
    {
      GECKO_INFO(::gecko::core::labels::Modules, "Shutdown: %s",
                 it->second.Root.Name ? it->second.Root.Name : "(unnamed)");
      m_impl->ShutdownModule(*this, it->second);
    }
  }

  m_impl->Booted = false;
}

bool ModuleRegistry::PublishServiceImpl(::gecko::ServiceId id, void* impl) noexcept
{
  if (impl == nullptr)
  {
    return false;
  }
  if (!m_impl)
  {
    m_impl.reset(new (::std::nothrow) Impl());
    if (!m_impl)
    {
      return false;
    }
  }
  if (m_impl->FindServiceImpl(id) != nullptr)
  {
    return false;
  }
  m_impl->Services.push_back(Impl::ServiceEntry {id, impl});
  return true;
}

void* ModuleRegistry::GetServiceImpl(::gecko::ServiceId id) const noexcept
{
  if (!m_impl)
  {
    return nullptr;
  }
  return m_impl->FindServiceImpl(id);
}

bool ModuleRegistry::UnpublishServiceImpl(::gecko::ServiceId id) noexcept
{
  if (!m_impl)
  {
    return false;
  }
  for (auto it = m_impl->Services.begin(); it != m_impl->Services.end(); ++it)
  {
    if (it->Id == id)
    {
      m_impl->Services.erase(it);
      return true;
    }
  }
  return false;
}
}  // namespace gecko::core::detail
