#include "depch.h"
#include "Core/ModuleRegistry.h"
#include "Project/Scene/Scene.h"
#include "Project/Project.h"

#include <algorithm>

namespace Fufu
{

// ── Helpers ───────────────────────────────────────────────────────────────────

std::shared_ptr<IModule> ModuleRegistry::find(std::string_view name) const
{
    auto it = std::find_if(m_Modules.begin(), m_Modules.end(),
        [name](const std::shared_ptr<IModule>& m) { return m->getName() == name; });
    return (it != m_Modules.end()) ? *it : nullptr;
}

// ── Registration ──────────────────────────────────────────────────────────────

void ModuleRegistry::add(std::shared_ptr<IModule> module)
{
    FUFU_ASSERT(module, "ModuleRegistry::add : module null");
    FUFU_ASSERT(!find(module->getName()),
                "ModuleRegistry::add : '{}' déjà enregistré", module->getName());
    m_Modules.push_back(std::move(module));
}

void ModuleRegistry::remove(std::string_view name)
{
    auto m = find(name);
    if (!m) return;
    if (m->isActive()) { m->onUnload(); m->m_Active = false; }
    m_Modules.erase(
        std::remove_if(m_Modules.begin(), m_Modules.end(),
            [name](const auto& p) { return p->getName() == name; }),
        m_Modules.end());
}

// ── Activation ────────────────────────────────────────────────────────────────

void ModuleRegistry::activate(std::string_view name)
{
    auto m = find(name);
    FUFU_ASSERT(m, "ModuleRegistry::activate : '{}' inconnu", std::string(name));
    if (m->isActive()) return;
    m->m_Active = true;
    m->onLoad();
}

void ModuleRegistry::deactivate(std::string_view name)
{
    auto m = find(name);
    if (!m || !m->isActive()) return;
    m->onUnload();
    m->m_Active = false;
}

void ModuleRegistry::deactivateAll()
{
    for (auto& m : m_Modules)
    {
        if (m->isActive()) { m->onUnload(); m->m_Active = false; }
    }
}

// ── Dispatch ──────────────────────────────────────────────────────────────────

void ModuleRegistry::tickAll(double dt)
{
    for (auto& m : m_Modules)
        if (m->isActive()) m->onTick(dt);
}

void ModuleRegistry::notifyProjectOpened(Project& project)
{
    for (auto& m : m_Modules)
        if (m->isActive()) m->onProjectOpened(project);
}

void ModuleRegistry::notifyProjectClosed()
{
    for (auto& m : m_Modules)
        if (m->isActive()) m->onProjectClosed();
}

void ModuleRegistry::notifySceneActivated(Scene& scene)
{
    for (auto& m : m_Modules)
        if (m->isActive()) m->onSceneActivated(scene);
}

void ModuleRegistry::notifySceneClosed()
{
    for (auto& m : m_Modules)
        if (m->isActive()) m->onSceneClosed();
}

// ── Lookup ────────────────────────────────────────────────────────────────────

IModule* ModuleRegistry::get(std::string_view name)
{
    auto m = find(name);
    return m ? m.get() : nullptr;
}

} // namespace Fufu
