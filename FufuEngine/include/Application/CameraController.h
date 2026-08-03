#pragma once

#include <glm/glm.hpp>
#include <Project/Scene/Scene.h>

namespace Fufu
{
    // Reusable FPS camera controller for any Fufu application
    // (FufuStudio, FufuLab, FufuRuntime...).
    //
    // Usage:
    //   1. Call onUpdate(dt, active) every frame.
    //      active = true if the application wants to capture input (viewport focused,
    //      no modal window, etc.) — false = the controller is silent.
    //   2. Call syncFromScene(scene) when a new scene becomes active,
    //      to initialize position/rotation from the existing camera entity.
    //   3. onUpdate calls syncToScene automatically if the camera moved.
    //      Alternatively, syncToScene can also be called manually.
    //
    // Returns true from onUpdate if the camera moved/rotated this frame
    // (the caller can then resetAccumulation on the Renderer).
    class CameraController
    {
    public:
        struct Settings
        {
            float moveSpeed       = 5.f;
            float lookSpeed       = 0.1f;
            float shiftMultiplier = 3.f;
        };

        bool onUpdate(float deltaTime, bool active = true);

        // true if a right-click was released without significant drag (context
        // menu). Consumed on read: a second call returns false.
        bool consumeContextMenuRequest();

        // Synchronizes position/rotation from the primary camera entity in the
        // scene: call when switching the active scene.
        void syncFromScene(Scene& scene);

        // Pushes position/rotation to the primary camera entity in the scene.
        // Called automatically by onUpdate if the camera has moved.
        void syncToScene(Scene& scene) const;

        Settings&       getSettings()       { return m_Settings; }
        const Settings& getSettings() const { return m_Settings; }

        glm::vec3 getPosition() const { return m_Position; }
        glm::vec3 getRotation() const { return m_Rotation; }
        void setPosition(const glm::vec3& p) { m_Position = p; }
        void setRotation(const glm::vec3& r) { m_Rotation = r; }

    private:
        glm::vec3 m_Position = { 0.f, 1.f, 5.f };
        glm::vec3 m_Rotation = { 0.f, 0.f, 0.f }; // pitch, yaw, roll (in radians)

        bool      m_FirstMouse              = true;
        glm::vec2 m_LastMousePos            = { 0.f, 0.f };
        float     m_RightClickDragDist      = 0.f;
        bool      m_RightWasDown            = false;
        bool      m_ContextMenuPending      = false;

        Settings  m_Settings;
    };
}
