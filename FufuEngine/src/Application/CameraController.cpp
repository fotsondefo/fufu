#include "depch.h"
#include "Application/CameraController.h"
#include "Application/Application.h"
#include "Project/Components.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Fufu
{
    bool CameraController::onUpdate(float deltaTime, bool active)
    {
        if (!active)
        {
            m_FirstMouse  = true;
            m_RightWasDown = false;
            return false;
        }

        GLFWwindow* window = static_cast<GLFWwindow*>(
            Application::get().getWindow().getNativeWindow());

        bool moved = false;

        // --- Rotation souris (clic droit maintenu) ---
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            glm::vec2 mousePos = { static_cast<float>(mx), static_cast<float>(my) };

            if (m_FirstMouse)
            {
                m_LastMousePos       = mousePos;
                m_FirstMouse         = false;
                m_RightClickDragDist = 0.f;
            }

            glm::vec2 delta    = mousePos - m_LastMousePos;
            m_LastMousePos     = mousePos;
            m_RightClickDragDist += glm::length(delta);

            m_Rotation.y -= delta.x * m_Settings.lookSpeed * deltaTime; // yaw
            m_Rotation.x -= delta.y * m_Settings.lookSpeed * deltaTime; // pitch

            m_Rotation.x = std::clamp(
                m_Rotation.x,
                glm::radians(-89.f),
                glm::radians(89.f)
            );

            if (glm::length(delta) > 0.f) moved = true;
            m_RightWasDown = true;
        }
        else
        {
            if (m_RightWasDown && m_RightClickDragDist < 4.f)
                m_ContextMenuPending = true;

            m_FirstMouse   = true;
            m_RightWasDown = false;
        }

        // --- Déplacement WASD ---
        glm::quat camRot  = glm::quat(glm::vec3(m_Rotation.x, m_Rotation.y, 0.f));
        glm::vec3 forward = glm::normalize(camRot * glm::vec3(0.f, 0.f, -1.f));
        glm::vec3 right   = glm::normalize(camRot * glm::vec3(1.f, 0.f, 0.f));
        glm::vec3 up      = glm::vec3(0.f, 1.f, 0.f);

        float speed = m_Settings.moveSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= m_Settings.shiftMultiplier;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { m_Position += forward * speed; moved = true; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { m_Position -= forward * speed; moved = true; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { m_Position -= right   * speed; moved = true; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { m_Position += right   * speed; moved = true; }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { m_Position += up      * speed; moved = true; }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { m_Position -= up      * speed; moved = true; }

        return moved;
    }

    bool CameraController::consumeContextMenuRequest()
    {
        bool v = m_ContextMenuPending;
        m_ContextMenuPending = false;
        return v;
    }

    void CameraController::syncFromScene(Scene& scene)
    {
        Entity cam = scene.getPrimaryCamera();
        if (!cam) return;

        auto& t    = cam.getComponent<TransformComponent>();
        m_Position = t.position;
        m_Rotation = t.rotation;
    }

    void CameraController::syncToScene(Scene& scene) const
    {
        Entity cam = scene.getPrimaryCamera();
        if (!cam) return;

        auto& t    = cam.getComponent<TransformComponent>();
        t.position = m_Position;
        t.rotation = m_Rotation;
    }
}
