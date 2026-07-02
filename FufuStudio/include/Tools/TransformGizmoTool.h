#pragma once

#include "IEditorTool.h"
#include "EditorState.h"
#include "Helpers/FontIcons.h"
#include "Commands/CommandHistory.h"
#include "Commands/ComponentCommands.h"
#include <Renderer/Renderer.h>
#include <Project/Components.h>
#include <optional>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace FufuStudio
{
	// Outil de manipulation Translate/Rotate/Scale via ImGuizmo sur l'entit� s�lectionn�e.
	class TransformGizmoTool : public IEditorTool
	{
	public:
		explicit TransformGizmoTool(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		const char* getName() const override { return "Transform"; }
		bool isUsing() const override { return ImGuizmo::IsUsing(); }

		void onToolbar(EditorState& state) override
		{
			auto opButton = [&](const char* icon, Operation op, const char* tooltip)
			{
				bool active = (m_Operation == op);
				if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));

				if (ImGui::Button(icon, ImVec2(28.f, 28.f)))
					m_Operation = op;

				if (active) ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
				ImGui::SameLine();
			};

			opButton(ICON_CI_ARROW_BOTH, Operation::Translate, "Translate (W)");
			opButton(ICON_CI_ARROW_CIRCLE_UP, Operation::Rotate, "Rotate (E)");
			opButton(ICON_FA_EXPAND, Operation::Scale, "Scale (R)");
		}

		void onShortcuts(EditorState& state) override
		{
			if (!state.viewportFocused || !state.viewportHovered) return;
			if (ImGuizmo::IsUsing()) return; // ne pas changer de mode pendant manipulation

			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_Operation = Operation::Translate;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) m_Operation = Operation::Rotate;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_Operation = Operation::Scale;

			// Tab pour basculer World/Local
			if (ImGui::IsKeyPressed(ImGuiKey_Tab))
				m_Space = (m_Space == Space::World) ? Space::Local : Space::World;
		}

		void onViewportOverlay(EditorState& state) override
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
			ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

			// Matrices caméra
			auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
			auto& camComponent = cam.getComponent<Fufu::CameraComponent>();

			glm::mat4 view = glm::inverse(camTransform.getTransform());
			float aspect = windowSize.x / windowSize.y;
			glm::mat4 proj = camComponent.getProjectionMatrix(aspect);

			// OpenGL → ImGuizmo attend Y inversé sur la projection
			proj[1][1] *= -1.f;

			// Matrice de l'entité sélectionnée
			auto& entityTransform = state.selectedEntity.getComponent<Fufu::TransformComponent>();
			glm::mat4 model = entityTransform.getTransform();

			// Tant que le gizmo n'est pas en cours d'utilisation, on garde un snapshot
			// à jour : c'est la valeur "avant" qu'on utilisera si un drag démarre.
			if (!m_GizmoWasUsing)
				m_GizmoBeforeEdit = entityTransform;

			ImGuizmo::OPERATION op;
			switch (m_Operation)
			{
			case Operation::Translate: op = ImGuizmo::TRANSLATE; break;
			case Operation::Rotate:    op = ImGuizmo::ROTATE;    break;
			case Operation::Scale:     op = ImGuizmo::SCALE;     break;
			}

			ImGuizmo::MODE mode = (m_Space == Space::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

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
				// Décomposer la matrice résultante en position/rotation/scale
				float translation[3], rotation[3], scale[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), translation, rotation, scale);

				entityTransform.position = glm::vec3(translation[0], translation[1], translation[2]);
				entityTransform.rotation = glm::vec3(
					glm::radians(rotation[0]), glm::radians(rotation[1]), glm::radians(rotation[2]));
				entityTransform.scale = glm::vec3(scale[0], scale[1], scale[2]);

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

	private:
		enum class Operation { Translate, Rotate, Scale };
		enum class Space { World, Local };

		Fufu::Renderer& m_Renderer;
		Operation m_Operation = Operation::Translate;
		Space m_Space = Space::World;

		// Tracking du drag pour l'undo : snapshot pris juste avant le début du
		// drag, commande poussée quand il se termine (voir onViewportOverlay).
		bool m_GizmoWasUsing = false;
		std::optional<Fufu::TransformComponent> m_GizmoBeforeEdit;
	};
}
