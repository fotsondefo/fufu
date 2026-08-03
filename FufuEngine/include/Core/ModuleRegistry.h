#pragma once

#include "Core/IModule.h"
#include <memory>
#include <vector>
#include <string_view>

namespace Fufu
{

class Scene;
class Project;

// Engine module registry.
//
// Responsibilities:
//   - Own the modules (shared_ptr)
//   - Activate / deactivate modules (onLoad / onUnload)
//   - Dispatch tick and project/scene notifications to active modules
//
// The notify* methods are called by Application, which is connected to the
// signals of ProjectManager and SceneManager. Modules know neither Application
// nor the registry — they simply receive their lifecycle hooks.
//
// Example:
//   auto& reg = Application::get().getModuleRegistry();
//   reg.add(std::make_shared<GameModule>());
//   reg.activate("Game");
//   // onProjectOpened / onSceneActivated arrive automatically afterwards
class ModuleRegistry
{
public:
    // ── Registration ──────────────────────────────────────────────────────────
    void add   (std::shared_ptr<IModule> module);
    void remove(std::string_view name);

    // ── Activation ────────────────────────────────────────────────────────────
    void activate  (std::string_view name);
    void deactivate(std::string_view name);
    void deactivateAll();

    // ── Lookup ────────────────────────────────────────────────────────────────
    IModule* get(std::string_view name);

    template<typename T>
    T* get(std::string_view name) { return dynamic_cast<T*>(get(name)); }

    // ── Dispatch (called by Application) ─────────────────────────────────────
    void tickAll(double dt);

    void notifyProjectOpened (Project& project);
    void notifyProjectClosed ();
    void notifySceneActivated(Scene& scene);
    void notifySceneClosed   ();

    // ── Iteration ─────────────────────────────────────────────────────────────
    const std::vector<std::shared_ptr<IModule>>& modules() const { return m_Modules; }

private:
    std::shared_ptr<IModule> find(std::string_view name) const;

    std::vector<std::shared_ptr<IModule>> m_Modules;
};

} // namespace Fufu
