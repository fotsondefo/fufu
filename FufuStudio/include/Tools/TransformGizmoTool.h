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
			auto scene = state.getActiveScene();
			if (!scene) return;

			Fufu::Entity cam = scene->getPrimaryCamera();
			if (!cam) return;

			// Zone de la vraie IMAGE rendue (pas la fenêtre du panneau, qui
			// inclut sa propre barre de titre) : sinon le gizmo (et le pick) se
			// désynchronise visuellement de ce qui est affiché.
			ImVec2 imagePos = ImVec2(state.viewportPos.x, state.viewportPos.y);
			ImVec2 imageSize = ImVec2(state.viewportSize.x, state.viewportSize.y);

			// Matrices caméra
			auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
			auto& camComponent = cam.getComponent<Fufu::CameraComponent>();

			glm::mat4 view = glm::inverse(camTransform.getTransform());
			float aspect = imageSize.x / imageSize.y;
			glm::mat4 proj = camComponent.getProjectionMatrix(aspect);
			// Pas de flip Y ici : l'orientation verticale est déjà corrigée au
			// niveau de l'affichage de la texture (UV inversé dans ImGui::Image,
			// voir ViewportPanel). Appliquer un flip ici en plus désynchronise
			// la position projetée du gizmo par rapport à l'objet réellement affiché.

			// Sélection d'objet : clic gauche dans le viewport, sauf s'il atterrit
			// sur le gizmo lui-même (le manipuler ne doit pas aussi changer la
			// sélection). ImGuizmo::IsOver() reflète le Manipulate() de LA frame
			// précédente — décalage d'une frame sans conséquence pratique.
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
						// Clic dans le vide : désélectionne, comme dans la
						// plupart des éditeurs 3D.
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

			// Le gizmo se positionne sur l'entité "primaire" (pivot du groupe).
			auto& primaryTransform = primary.getComponent<Fufu::TransformComponent>();
			glm::mat4 oldModel = primaryTransform.getTransform();
			glm::mat4 model = oldModel;

			// Tant que le gizmo n'est pas en cours d'utilisation, on garde un snapshot
			// à jour de TOUTE la sélection : c'est la valeur "avant" utilisée si un drag démarre.
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
				applyDecomposedTransform(model, primaryTransform);

				// Répercute le même delta (autour du pivot primaire) sur le reste
				// de la sélection, pour un déplacement de groupe cohérent.
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

				// TransformComponent édité en place (pas d'add/removeComponent,
				// donc pas couvert par les hooks structurels d'Entity) : le GPUScene
				// doit être re-uploadé pour que l'instance bouge dans le rendu.
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

		// Tracking du drag pour l'undo : snapshot de toute la sélection pris juste
		// avant le début du drag, commande unique poussée quand il se termine.
		bool m_GizmoWasUsing = false;
		std::vector<std::pair<Fufu::Entity, Fufu::TransformComponent>> m_GroupBeforeEdit;
	};
}
