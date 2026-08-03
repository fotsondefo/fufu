#pragma once

#include "IEditorTool.h"
#include "EditorState.h"
#include "Helpers/FontIcons.h"
#include "Helpers/MeshPicking.h"
#include "Commands/CommandHistory.h"
#include "Commands/CompositeCommand.h"
#include "Commands/ComponentCommands.h"
#include <Renderer/Renderer.h>
#include <Project/Components.h>
#include <memory>
#include <utility>
#include <vector>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace FufuStudio
{
	// Translate/Rotate/Scale manipulation tool via ImGuizmo on the selected entity.
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
			if (ImGuizmo::IsUsing()) return; // do not switch mode during manipulation

			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_Operation = Operation::Translate;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) m_Operation = Operation::Rotate;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_Operation = Operation::Scale;

			// Tab to toggle World/Local
			if (ImGui::IsKeyPressed(ImGuiKey_Tab))
				m_Space = (m_Space == Space::World) ? Space::Local : Space::World;
		}

		void onViewportOverlay(EditorState& state) override
		{
			auto scene = state.getActiveScene();
			if (!scene) return;

			Fufu::Entity cam = scene->getPrimaryCamera();
			if (!cam) return;

			// Region of the actual rendered IMAGE (not the ImGui panel window, which
			// includes its own title bar): otherwise the gizmo (and pick) becomes
			// visually out of sync with what is displayed.
			ImVec2 imagePos = ImVec2(state.viewportPos.x, state.viewportPos.y);
			ImVec2 imageSize = ImVec2(state.viewportSize.x, state.viewportSize.y);

			// Camera matrices
			auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
			auto& camComponent = cam.getComponent<Fufu::CameraComponent>();

			glm::mat4 view = glm::inverse(camTransform.getTransform());
			float aspect = imageSize.x / imageSize.y;
			glm::mat4 proj = camComponent.getProjectionMatrix(aspect);
			// No Y flip here: the vertical orientation is already corrected at
			// the texture display level (UV inverted in ImGui::Image,
			// see ViewportPanel). Applying an additional flip here desyncs
			// the projected gizmo position relative to the actually displayed object.

			// Object selection: left-click in the viewport, unless it lands
			// on the gizmo itself (manipulating it should not also change the
			// selection). ImGuizmo::IsOver() reflects the Manipulate() of THE previous
			// frame — one-frame offset with no practical consequence.
			bool clickOnGizmo = !state.selection.empty() && ImGuizmo::IsOver();
			if (state.viewportHovered && !clickOnGizmo && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				glm::vec2 uv = {
					(mouse.x - imagePos.x) / imageSize.x,
					(mouse.y - imagePos.y) / imageSize.y
				};

				if (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f)
				{
					auto hit = pickEntity(*scene, proj * view, uv);
					if (ImGui::GetIO().KeyCtrl)
					{
						if (hit) state.selection.toggle(*hit);
					}
					else if (hit)
					{
						state.selection.select(*hit);
					}
					else
					{
						// Click in empty space: deselects, as in most 3D editors.
						state.selection.clear();
					}
				}
			}

			if (state.selection.empty())
				return;

			Fufu::Entity primary = state.selection.primary();
			if (!primary || !primary.isValid() || !primary.hasComponent<Fufu::TransformComponent>())
				return;

			ImGuizmo::BeginFrame();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

			// The gizmo is positioned on the "primary" entity (group pivot).
			auto& primaryTransform = primary.getComponent<Fufu::TransformComponent>();
			glm::mat4 oldModel = primaryTransform.getTransform();
			glm::mat4 model = oldModel;

			// While the gizmo is not in use, keep an up-to-date snapshot
			// of the ENTIRE selection: this is the "before" value used if a drag starts.
			if (!m_GizmoWasUsing)
			{
				m_GroupBeforeEdit.clear();
				for (Fufu::Entity e : state.selection.entities())
				{
					if (e.isValid() && e.hasComponent<Fufu::TransformComponent>())
						m_GroupBeforeEdit.emplace_back(e, e.getComponent<Fufu::TransformComponent>());
				}
			}

			ImGuizmo::OPERATION op;
			switch (m_Operation)
			{
			case Operation::Translate: op = ImGuizmo::TRANSLATE; break;
			case Operation::Rotate:    op = ImGuizmo::ROTATE;    break;
			case Operation::Scale:     op = ImGuizmo::SCALE;     break;
			}

			ImGuizmo::MODE mode = (m_Space == Space::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

			// Scale only supports LOCAL in ImGuizmo
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
				applyDecomposedTransform(model, primaryTransform);

				// Propagates the same delta (around the primary pivot) to the rest
				// of the selection, for a coherent group movement.
				if (state.selection.size() > 1)
				{
					glm::mat4 delta = model * glm::inverse(oldModel);

					for (Fufu::Entity e : state.selection.entities())
					{
						if (e == primary || !e.isValid() || !e.hasComponent<Fufu::TransformComponent>())
							continue;

						auto& t = e.getComponent<Fufu::TransformComponent>();
						glm::mat4 newModel = delta * t.getTransform();
						applyDecomposedTransform(newModel, t);
					}
				}

				// TransformComponent edited in place (no add/removeComponent,
				// so not covered by Entity's structural hooks): the GPUScene
				// must be re-uploaded for the instance to move in the render.
				scene->markDirty();
				m_Renderer.resetAccumulation();
			}

			bool usingNow = ImGuizmo::IsUsing();
			if (m_GizmoWasUsing && !usingNow && !m_GroupBeforeEdit.empty())
			{
				auto composite = std::make_unique<CompositeCommand>("Transform Selection");
				for (auto& [entity, before] : m_GroupBeforeEdit)
				{
					if (entity.isValid())
					{
						composite->add(std::make_unique<ComponentEditCommand<Fufu::TransformComponent>>(
							entity, before, entity.getComponent<Fufu::TransformComponent>()));
					}
				}

				if (!composite->empty())
					state.commandHistory->execute(std::move(composite));

				m_GroupBeforeEdit.clear();
			}
			m_GizmoWasUsing = usingNow;
		}

	private:
		enum class Operation { Translate, Rotate, Scale };
		enum class Space { World, Local };

		static void applyDecomposedTransform(const glm::mat4& model, Fufu::TransformComponent& transform)
		{
			float translation[3], rotation[3], scale[3];
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), translation, rotation, scale);

			transform.position = glm::vec3(translation[0], translation[1], translation[2]);
			transform.rotation = glm::vec3(
				glm::radians(rotation[0]), glm::radians(rotation[1]), glm::radians(rotation[2]));
			transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
		}

		Fufu::Renderer& m_Renderer;
		Operation m_Operation = Operation::Translate;
		Space m_Space = Space::World;

		// Drag tracking for undo: snapshot of the entire selection taken just
		// before the drag starts, a single command pushed when it ends.
		bool m_GizmoWasUsing = false;
		std::vector<std::pair<Fufu::Entity, Fufu::TransformComponent>> m_GroupBeforeEdit;
	};
}
