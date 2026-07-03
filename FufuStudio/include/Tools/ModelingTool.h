#pragma once

#include "IEditorTool.h"
#include "EditorState.h"
#include "Commands/CommandHistory.h"
#include "Commands/ExtrudeFaceCommand.h"
#include "Helpers/MeshPicking.h"
#include <Project/Components.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <optional>

namespace FufuStudio
{
	// Sélection et extrusion de faces sur le mesh de l'entité sélectionnée.
	// Une "face" est un triangle du buffer d'indices — pas de fusion en
	// n-gones coplanaires (limitation assumée pour cette première itération).
	// Le pick et l'extrude n'opèrent que sur le premier sous-maillage.
	class ModelingTool : public IEditorTool
	{
	public:
		const char* getName() const override { return "Model"; }

		void onToolbar(EditorState& state) override
		{
			ImGui::TextUnformatted(m_SelectedFace.has_value()
				? "Face selected"
				: "Click a face on the mesh");

			ImGui::SetNextItemWidth(140.f);
			ImGui::DragFloat("##extrudeDist", &m_ExtrudeDistance, 0.01f, -10.f, 10.f, "Dist: %.2f");
			ImGui::SameLine();

			Fufu::Entity entity = state.selection.primary();
			bool canExtrude = m_SelectedFace.has_value()
				&& entity && entity.isValid() && entity.hasComponent<Fufu::MeshComponent>();

			if (!canExtrude) ImGui::BeginDisabled();
			if (ImGui::Button("Extrude"))
				extrude(state, entity);
			if (!canExtrude) ImGui::EndDisabled();
		}

		void onViewportOverlay(EditorState& state) override
		{
			Fufu::Entity entity = state.selection.primary();
			if (!entity || !entity.isValid() || !entity.hasComponent<Fufu::MeshComponent>())
			{
				m_SelectedFace.reset();
				return;
			}

			auto scene = state.getActiveScene();
			if (!scene) return;

			Fufu::Entity cam = scene->getPrimaryCamera();
			if (!cam) return;

			ImVec2 imagePos = ImVec2(state.viewportPos.x, state.viewportPos.y);
			ImVec2 imageSize = ImVec2(state.viewportSize.x, state.viewportSize.y);

			auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
			auto& camComponent = cam.getComponent<Fufu::CameraComponent>();
			glm::mat4 view = glm::inverse(camTransform.getTransform());
			float aspect = imageSize.x / imageSize.y;
			glm::mat4 viewProj = camComponent.getProjectionMatrix(aspect) * view;

			auto meshAsset = getMeshAssetForEntity(entity);
			if (!meshAsset || meshAsset->getSubMeshCount() == 0)
				return;

			const Fufu::SubMesh& mesh = meshAsset->getSubMeshes()[0];
			glm::mat4 model = entity.getComponent<Fufu::TransformComponent>().getTransform();

			// Pick au clic gauche
			if (state.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				glm::vec2 uv = {
					(mouse.x - imagePos.x) / imageSize.x,
					(mouse.y - imagePos.y) / imageSize.y
				};

				if (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f)
				{
					MeshPickResult pick = pickMesh(mesh, model, viewProj, uv);
					m_SelectedFace = pick.hit ? std::optional<std::size_t>(pick.faceIndex) : std::nullopt;
				}
			}

			// Surbrillance de la face sélectionnée
			if (m_SelectedFace.has_value())
			{
				std::size_t i0 = *m_SelectedFace * 3;
				if (i0 + 2 < mesh.indices.size())
				{
					ImVec2 screenPts[3];
					bool visible = true;

					for (int i = 0; i < 3 && visible; ++i)
					{
						glm::vec3 worldPos = glm::vec3(model * glm::vec4(
							mesh.vertices[mesh.indices[i0 + static_cast<std::size_t>(i)]].position, 1.f));

						auto screen = worldToScreen(worldPos, viewProj, imagePos, imageSize);
						if (!screen) { visible = false; break; }
						screenPts[i] = *screen;
					}

					if (visible)
					{
						auto* drawList = ImGui::GetWindowDrawList();
						drawList->AddTriangleFilled(screenPts[0], screenPts[1], screenPts[2], IM_COL32(255, 165, 0, 90));
						drawList->AddTriangle(screenPts[0], screenPts[1], screenPts[2], IM_COL32(255, 165, 0, 255), 2.f);
					}
				}
			}
		}

	private:
		void extrude(EditorState& state, Fufu::Entity entity)
		{
			if (!m_SelectedFace.has_value() || !entity || !entity.isValid())
				return;

			auto& meshComp = entity.getComponent<Fufu::MeshComponent>();
			state.commandHistory->executeCommand<ExtrudeFaceCommand>(
				std::filesystem::path(meshComp.meshPath), std::size_t(0), *m_SelectedFace, m_ExtrudeDistance);

			// La zone extrudée n'est plus la même géométriquement : on
			// désélectionne pour éviter d'extruder deux fois par erreur.
			m_SelectedFace.reset();
		}

		std::optional<std::size_t> m_SelectedFace;
		float m_ExtrudeDistance = 0.5f;
	};
}
