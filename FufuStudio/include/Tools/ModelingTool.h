#pragma once

#include "IEditorTool.h"
#include "EditorState.h"
#include "Commands/CommandHistory.h"
#include "Commands/ExtrudeFaceCommand.h"
#include <Project/Components.h>
#include <Project/Assets/MeshAsset.h>
#include <Application/Application.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <optional>
#include <limits>

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

			// Zone de la vraie IMAGE rendue (pas la fenêtre du panneau, qui
			// inclut sa propre barre de titre) : sinon le pick vise à côté.
			ImVec2 windowPos = ImVec2(state.viewportPos.x, state.viewportPos.y);
			ImVec2 windowSize = ImVec2(state.viewportSize.x, state.viewportSize.y);

			auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
			auto& camComponent = cam.getComponent<Fufu::CameraComponent>();
			glm::mat4 view = glm::inverse(camTransform.getTransform());
			float aspect = windowSize.x / windowSize.y;
			glm::mat4 viewProj = camComponent.getProjectionMatrix(aspect) * view;

			auto meshAsset = getMeshAsset(entity);
			if (!meshAsset || meshAsset->getSubMeshCount() == 0)
				return;

			const Fufu::SubMesh& mesh = meshAsset->getSubMeshes()[0];
			glm::mat4 model = entity.getComponent<Fufu::TransformComponent>().getTransform();

			// Pick au clic gauche
			if (state.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				glm::vec2 uv = {
					(mouse.x - windowPos.x) / windowSize.x,
					(mouse.y - windowPos.y) / windowSize.y
				};

				if (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f)
					m_SelectedFace = pickFace(mesh, model, viewProj, uv);
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

						auto screen = worldToScreen(worldPos, viewProj, windowPos, windowSize);
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
		static std::optional<ImVec2> worldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj,
			ImVec2 windowPos, ImVec2 windowSize)
		{
			glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.f);
			if (clip.w <= 0.f) return std::nullopt;

			glm::vec2 ndc = glm::vec2(clip) / clip.w;
			return ImVec2(
				windowPos.x + (ndc.x * 0.5f + 0.5f) * windowSize.x,
				windowPos.y + (1.f - (ndc.y * 0.5f + 0.5f)) * windowSize.y
			);
		}

		// Pick approximatif en espace écran : projette chaque triangle, teste
		// l'appartenance barycentrique au point cliqué, garde le plus proche
		// (clip.w comme proxy de profondeur). Pas de vrai lancer de rayon 3D
		// nécessaire pour un usage éditeur sur des maillages de cette taille.
		static std::optional<std::size_t> pickFace(const Fufu::SubMesh& mesh, const glm::mat4& model,
			const glm::mat4& viewProj, glm::vec2 uv)
		{
			glm::vec2 ndcClick = { uv.x * 2.f - 1.f, 1.f - uv.y * 2.f };

			std::optional<std::size_t> best;
			float bestDepth = std::numeric_limits<float>::max();

			for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
			{
				glm::vec4 clipA = viewProj * model * glm::vec4(mesh.vertices[mesh.indices[i]].position, 1.f);
				glm::vec4 clipB = viewProj * model * glm::vec4(mesh.vertices[mesh.indices[i + 1]].position, 1.f);
				glm::vec4 clipC = viewProj * model * glm::vec4(mesh.vertices[mesh.indices[i + 2]].position, 1.f);

				if (clipA.w <= 0.f || clipB.w <= 0.f || clipC.w <= 0.f)
					continue;

				glm::vec2 ndcA = glm::vec2(clipA) / clipA.w;
				glm::vec2 ndcB = glm::vec2(clipB) / clipB.w;
				glm::vec2 ndcC = glm::vec2(clipC) / clipC.w;

				float denom = (ndcB.y - ndcC.y) * (ndcA.x - ndcC.x) + (ndcC.x - ndcB.x) * (ndcA.y - ndcC.y);
				if (glm::abs(denom) < 1e-8f) continue;

				float w0 = ((ndcB.y - ndcC.y) * (ndcClick.x - ndcC.x) + (ndcC.x - ndcB.x) * (ndcClick.y - ndcC.y)) / denom;
				float w1 = ((ndcC.y - ndcA.y) * (ndcClick.x - ndcC.x) + (ndcA.x - ndcC.x) * (ndcClick.y - ndcC.y)) / denom;
				float w2 = 1.f - w0 - w1;

				if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;

				float depth = w0 * clipA.w + w1 * clipB.w + w2 * clipC.w;
				if (depth < bestDepth)
				{
					bestDepth = depth;
					best = i / 3;
				}
			}

			return best;
		}

		static std::shared_ptr<Fufu::MeshAsset> getMeshAsset(Fufu::Entity entity)
		{
			auto& mesh = entity.getComponent<Fufu::MeshComponent>();
			auto& pm = Fufu::Application::get().getProjectManager();
			if (!pm.hasProject()) return nullptr;
			return pm.getCurrentProject().getAssetManager().getMesh(mesh.meshPath);
		}

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
