#pragma once

#include <glm/glm.hpp>
#include <Project/Scene/Scene.h>

namespace Fufu
{
    // Contrôleur caméra FPS réutilisable par toute application Fufu
    // (FufuStudio, FufuLab, FufuRuntime...).
    //
    // Utilisation :
    //   1. Appeler onUpdate(dt, active) chaque frame.
    //      active = true si l'application veut capturer l'input (viewport focused,
    //      pas de fenêtre modale, etc.) — false = le contrôleur est silencieux.
    //   2. Appeler syncFromScene(scene) quand une nouvelle scène devient active,
    //      pour initialiser position/rotation depuis l'entité caméra existante.
    //   3. onUpdate appelle syncToScene automatiquement si la caméra a bougé.
    //      Sinon, syncToScene est aussi appelable manuellement.
    //
    // Retourne true depuis onUpdate si la caméra a bougé/tourné ce frame
    // (l'appelant peut alors resetAccumulation sur le Renderer).
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

        // true si un clic droit a été relâché sans drag significatif (menu
        // contextuel). Consommé à la lecture : un second appel renvoie false.
        bool consumeContextMenuRequest();

        // Synchronise position/rotation depuis l'entité caméra primaire de la
        // scène : à appeler quand on change de scène active.
        void syncFromScene(Scene& scene);

        // Pousse position/rotation vers l'entité caméra primaire de la scène.
        // Appelé automatiquement par onUpdate si la caméra a bougé.
        void syncToScene(Scene& scene) const;

        Settings&       getSettings()       { return m_Settings; }
        const Settings& getSettings() const { return m_Settings; }

        glm::vec3 getPosition() const { return m_Position; }
        glm::vec3 getRotation() const { return m_Rotation; }
        void setPosition(const glm::vec3& p) { m_Position = p; }
        void setRotation(const glm::vec3& r) { m_Rotation = r; }

    private:
        glm::vec3 m_Position = { 0.f, 1.f, 5.f };
        glm::vec3 m_Rotation = { 0.f, 0.f, 0.f }; // pitch, yaw, roll (radians)

        bool      m_FirstMouse              = true;
        glm::vec2 m_LastMousePos            = { 0.f, 0.f };
        float     m_RightClickDragDist      = 0.f;
        bool      m_RightWasDown            = false;
        bool      m_ContextMenuPending      = false;

        Settings  m_Settings;
    };
}
