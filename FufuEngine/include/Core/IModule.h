#pragma once

#include "Core/Timeline.h"

namespace Fufu
{

class Scene;     // forward — implementations include Scene.h
class Project;   // forward — implementations include Project.h
class ModuleRegistry;

// Neutral domain interface.
//
// A module encapsulates a complete business paradigm (game, motion design,
// simulation, audio...) without the core knowing which one.
//
// Lifecycle:
//   onLoad()               — module activated (no scene/project imposed here)
//   onProjectOpened(p)     — a project has just been opened or created
//   onSceneActivated(s)    — the active scene changed in the current project
//   onSceneClosed()        — the current scene is unloaded
//   onProjectClosed()      — the current project is closed
//   onUnload()             — module deactivated
//
// The module queries Application::get() to access the resources it needs
// (assets, renderer...). It receives nothing via injection in onLoad because
// its lifecycle is independent of a specific project or scene.
//
// What IModule IS NOT:
//   - An ILayer: layers manage UI and input.
//   - A singleton: multiple modules can coexist (e.g. Game + Audio).
class IModule
{
public:
    virtual ~IModule() = default;

    // ── Identity ──────────────────────────────────────────────────────────────
    virtual const char* getName() const = 0;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void onLoad()    {}
    virtual void onUnload()  {}

    virtual void onProjectOpened (Project& project) {}
    virtual void onProjectClosed ()                 {}

    virtual void onSceneActivated(Scene& scene) {}
    virtual void onSceneClosed   ()             {}

    // ── Update ────────────────────────────────────────────────────────────────
    virtual void onTick(double dt) { m_Timeline.tick(dt); }

    // ── Timeline ──────────────────────────────────────────────────────────────
    Timeline&       getTimeline()       { return m_Timeline; }
    const Timeline& getTimeline() const { return m_Timeline; }

    // ── State ─────────────────────────────────────────────────────────────────
    bool isActive() const { return m_Active; }

protected:
    Timeline m_Timeline;

private:
    bool m_Active = false;
    friend class ModuleRegistry;
};

} // namespace Fufu
