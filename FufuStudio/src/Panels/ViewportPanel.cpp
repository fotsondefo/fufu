#include "Panels/ViewportPanel.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <Application/Application.h>
#include <Project/Components.h>
#include <algorithm>

namespace FufuStudio 
{

	void ViewportPanel::onUpdate(EditorState& state, float deltaTime)
	{
		if (state.viewportFocused)
			handleCameraInput(state, deltaTime);

		syncCameraToScene(state);
	}

	void ViewportPanel::handleCameraInput(EditorState& state, float deltaTime)
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Fufu::Application::get().getWindow().getNativeWindow());

		// --- Rotation souris (clic droit maintenu) ---
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			double mx, my;
			glfwGetCursorPos(window, &mx, &my);
			glm::vec2 mousePos = { static_cast<float>(mx), static_cast<float>(my) };

			if (m_FirstMouse)
			{
				m_LastMousePos = mousePos;
				m_FirstMouse = false;
			}

			glm::vec2 delta = mousePos - m_LastMousePos;
			m_LastMousePos = mousePos;

			state.cameraRotation.y += delta.x * state.cameraLookSpeed * deltaTime; // yaw
			state.cameraRotation.x += delta.y * state.cameraLookSpeed * deltaTime; // pitch

			// Clamp pitch pour éviter le gimbal lock
			state.cameraRotation.x = std::clamp(
				state.cameraRotation.x,
				glm::radians(-89.f),
				glm::radians(89.f)
			);

			m_Renderer.resetAccumulation();
		}
		else
		{
			m_FirstMouse = true;
		}

		// --- Déplacement WASD ---
		float pitch = state.cameraRotation.x;
		float yaw = state.cameraRotation.y;

		glm::vec3 forward = glm::normalize(glm::vec3(
			glm::cos(pitch) * glm::sin(yaw),
			-glm::sin(pitch),
			glm::cos(pitch) * glm::cos(yaw)
		));
		glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
		glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);

		float speed = state.cameraMoveSpeed * deltaTime;

		// Shift pour accélérer
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 3.f;

		bool moved = false;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { state.cameraPosition += forward * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { state.cameraPosition -= forward * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { state.cameraPosition -= right * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { state.cameraPosition += right * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { state.cameraPosition += up * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { state.cameraPosition -= up * speed; moved = true; }

		if (moved) m_Renderer.resetAccumulation();
	}

	void ViewportPanel::syncCameraToScene(EditorState& state)
	{
		if (!state.getActiveScene()) return;

		Fufu::Entity cam = state.getActiveScene()->getPrimaryCamera();
		if (!cam) return;

		auto& t = cam.getComponent<Fufu::TransformComponent>();
		t.position = state.cameraPosition;
		t.rotation = state.cameraRotation;
	}

	void ViewportPanel::onImGuiRender(EditorState& state)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::Begin(ICON_FA_EYE " Viewport##viewport");

		state.viewportFocused = ImGui::IsWindowFocused();
		state.viewportHovered = ImGui::IsWindowHovered();

		// Détecter un redimensionnement du viewport
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (viewportSize.x > 0 && viewportSize.y > 0)
		{
			glm::vec2 newSize = { viewportSize.x, viewportSize.y };
			if (newSize != state.viewportSize)
			{
				state.viewportSize = newSize;
				m_Renderer.resize(
					static_cast<int>(newSize.x),
					static_cast<int>(newSize.y)
				);
			}
		}

		// Afficher la texture du renderer comme image ImGui
		ImTextureID texID = reinterpret_cast<ImTextureID>(
			static_cast<uintptr_t>(m_Renderer.getOutputTextureID())
			);
		ImGui::Image(texID,
			viewportSize,
			ImVec2(0, 1),   // UV flip vertical — OpenGL origin bas-gauche
			ImVec2(1, 0)
		);

		// Overlay : infos accumulation
		if (m_Renderer.getSettings().mode == Fufu::RenderMode::Accumulation)
		{
			ImVec2 overlayPos = ImGui::GetWindowPos();
			overlayPos.x += 8.f;
			overlayPos.y += 8.f;
			ImGui::GetWindowDrawList()->AddText(
				overlayPos, IM_COL32(255, 255, 255, 200),
				("Samples: " + std::to_string(m_Renderer.getAccumulatedFrames())).c_str()
			);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

} // namespace FufuStudio