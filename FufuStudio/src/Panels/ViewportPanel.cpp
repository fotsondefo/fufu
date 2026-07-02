#include "Panels/ViewportPanel.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <Application/Application.h>
#include <Project/Components.h>
#include <algorithm>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include "Commands/CommandHistory.h"
#include "Commands/ComponentCommands.h"

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

			// Clamp pitch pour �viter le gimbal lock
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

		// --- D�placement WASD ---
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

		// Shift pour acc�l�rer
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

		// Toolbar overlay en haut � gauche du viewport
		ImGui::SetCursorPos(ImVec2(8.f, 8.f));
		ImGui::BeginGroup();

		auto opButton = [&](const char* icon, EditorState::GizmoOperation op, const char* tooltip)
		{
			bool active = (state.gizmoOperation == op);
			if (active) ImGui::PushStyleColor(ImGuiCol_Button,
				ImVec4(0.26f, 0.59f, 0.98f, 1.f));

			if (ImGui::Button(icon, ImVec2(28.f, 28.f)))
				state.gizmoOperation = op;

			if (active) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
			ImGui::SameLine();
		};

		opButton(ICON_CI_ARROW_BOTH,
			EditorState::GizmoOperation::Translate, "Translate (W)");
		opButton(ICON_CI_ARROW_CIRCLE_UP,
			EditorState::GizmoOperation::Rotate, "Rotate (E)");
		opButton(ICON_FA_EXPAND,
			EditorState::GizmoOperation::Scale, "Scale (R)");

		ImGui::EndGroup();

		state.viewportFocused = ImGui::IsWindowFocused();
		state.viewportHovered = ImGui::IsWindowHovered();

		// D�tecter un redimensionnement du viewport
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

		ImVec2 imagePos = ImGui::GetCursorScreenPos();

		ImTextureID texID = reinterpret_cast<ImTextureID>(
			static_cast<uintptr_t>(m_Renderer.getOutputTextureID())
		);
		ImGui::Image(
			texID,
			viewportSize,
			ImVec2(0, 1),   // UV flip vertical � OpenGL origin bas-gauche
			ImVec2(1, 0)
		);

		drawGizmo(state);
		handleGizmoShortcuts(state);

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

	void ViewportPanel::handleGizmoShortcuts(EditorState& state)
	{
		if (!state.viewportFocused || !state.viewportHovered) return;
		if (ImGuizmo::IsUsing()) return; // ne pas changer de mode pendant manipulation

		if (ImGui::IsKeyPressed(ImGuiKey_W))
			state.gizmoOperation = EditorState::GizmoOperation::Translate;
		if (ImGui::IsKeyPressed(ImGuiKey_E))
			state.gizmoOperation = EditorState::GizmoOperation::Rotate;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			state.gizmoOperation = EditorState::GizmoOperation::Scale;

		// Tab pour basculer World/Local
		if (ImGui::IsKeyPressed(ImGuiKey_Tab))
		{
			state.gizmoSpace = (state.gizmoSpace == EditorState::GizmoSpace::World)
				? EditorState::GizmoSpace::Local
				: EditorState::GizmoSpace::World;
		}
	}

	void ViewportPanel::drawGizmo(EditorState& state)
	{
		if (!state.selectedEntity || !state.selectedEntity.isValid())
			return;

		if (!state.selectedEntity.hasComponent<Fufu::TransformComponent>())
			return;

		auto scene = state.getActiveScene();
		if (!scene) return;

		Fufu::Entity cam = scene->getPrimaryCamera();
		if (!cam) return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImGuizmo::SetRect(windowPos.x, windowPos.y,
			windowSize.x, windowSize.y);

		// Matrices cam�ra
		auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
		auto& camComponent = cam.getComponent<Fufu::CameraComponent>();

		glm::mat4 view = glm::inverse(camTransform.getTransform());
		float aspect = windowSize.x / windowSize.y;
		glm::mat4 proj = camComponent.getProjectionMatrix(aspect);

		// OpenGL ? ImGuizmo attend Y invers� sur la projection
		proj[1][1] *= -1.f;

		// Matrice de l'entit� s�lectionn�e
		auto& entityTransform = state.selectedEntity
			.getComponent<Fufu::TransformComponent>();
		glm::mat4 model = entityTransform.getTransform();

		// Tant que le gizmo n'est pas en cours d'utilisation, on garde un snapshot
		// � jour : c'est la valeur "avant" qu'on utilisera si un drag d�marre.
		if (!m_GizmoWasUsing)
			m_GizmoBeforeEdit = entityTransform;

		ImGuizmo::OPERATION op;
		switch (state.gizmoOperation)
		{
		case EditorState::GizmoOperation::Translate:
			op = ImGuizmo::TRANSLATE; break;
		case EditorState::GizmoOperation::Rotate:
			op = ImGuizmo::ROTATE; break;
		case EditorState::GizmoOperation::Scale:
			op = ImGuizmo::SCALE; break;
		}

		ImGuizmo::MODE mode = (state.gizmoSpace == EditorState::GizmoSpace::World)
			? ImGuizmo::WORLD : ImGuizmo::LOCAL;

		// Scale ne supporte que LOCAL dans ImGuizmo
		if (op == ImGuizmo::SCALE)
			mode = ImGuizmo::LOCAL;

		bool manipulated = ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
			op, mode,
			glm::value_ptr(model)
		);

		if (manipulated)
		{
			// D�composer la matrice r�sultante en position/rotation/scale
			float translation[3], rotation[3], scale[3];
			ImGuizmo::DecomposeMatrixToComponents(
				glm::value_ptr(model), translation, rotation, scale);

			entityTransform.position = glm::vec3(
				translation[0], translation[1], translation[2]);
			entityTransform.rotation = glm::vec3(
				glm::radians(rotation[0]),
				glm::radians(rotation[1]),
				glm::radians(rotation[2]));
			entityTransform.scale = glm::vec3(
				scale[0], scale[1], scale[2]);

			m_Renderer.resetAccumulation();
		}

		bool usingNow = ImGuizmo::IsUsing();
		if (m_GizmoWasUsing && !usingNow && m_GizmoBeforeEdit.has_value())
		{
			state.commandHistory->executeCommand<ComponentEditCommand<Fufu::TransformComponent>>(
				state.selectedEntity, *m_GizmoBeforeEdit, entityTransform);
			m_GizmoBeforeEdit.reset();
		}
		m_GizmoWasUsing = usingNow;
	}

} // namespace FufuStudio