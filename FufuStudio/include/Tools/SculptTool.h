#pragma once

#include "IEditorTool.h"
#include "EditorState.h"
#include "Helpers/MeshPicking.h"
#include "Commands/CommandHistory.h"
#include "Commands/SculptStrokeCommand.h"
#include <Renderer/Renderer.h>
#include <Project/Components.h>
#include <Project/Assets/MeshUtils.h>
#include <imgui.h>
#include <glm/glm.hpp>

namespace FufuStudio
{
	// Displacement sculpt: pushes/pulls vertices along their normal within
	// a radius around the hovered point, while the left mouse button is held.
	// Only operates on the first sub-mesh of the selected entity
	// (same limitation as ModelingTool).
	//
	// Assumed limitation: vertices are not welded between faces (our
	// hard-normal primitives like Cube share none), so sculpting near an edge
	// can create a visible crack. Works well on shared-vertex topology (Sphere, Cylinder).
	class SculptTool : public IEditorTool
	{
	public:
		explicit SculptTool(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		const char* getName() const override { return "Sculpt"; }
		bool isUsing() const override { return m_Dragging; }

		void onToolbar(EditorState& state) override
		{
			(void)state;

			if (ImGui::Button(m_Mode == Mode::Push ? "Push" : "Pull"))
				m_Mode = (m_Mode == Mode::Push) ? Mode::Pull : Mode::Push;

			ImGui::SameLine();
			ImGui::SetNextItemWidth(130.f);
			ImGui::DragFloat("##sculptRadius", &m_Radius, 0.01f, 0.05f, 5.f, "Radius: %.2f");

			ImGui::SameLine();
			ImGui::SetNextItemWidth(130.f);
			ImGui::DragFloat("##sculptStrength", &m_Strength, 0.005f, 0.01f, 2.f, "Strength: %.2f");
		}

		void onViewportOverlay(EditorState& state) override
		{
			Fufu::Entity entity = state.selection.primary();
			if (!entity || !entity.isValid() || !entity.hasComponent<Fufu::MeshComponent>())
			{
				endStroke(state, entity);
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

			glm::mat4 model = entity.getComponent<Fufu::TransformComponent>().getTransform();

			MeshPickResult pick;
			if (state.viewportHovered)
			{
				ImVec2 mouse = ImGui::GetMousePos();
				glm::vec2 uv = {
					(mouse.x - imagePos.x) / imageSize.x,
					(mouse.y - imagePos.y) / imageSize.y
				};

				if (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f)
					pick = pickMesh(meshAsset->getSubMeshes()[0], model, viewProj, uv);
			}

			// Brush cursor (circle projected on screen, radius measured via the
			// camera "right" vector — approximate but sufficient for visual feedback)
			if (pick.hit)
			{
				glm::vec3 camRight = glm::vec3(glm::inverse(view)[0]);
				auto centerScreen = worldToScreen(pick.worldPosition, viewProj, imagePos, imageSize);
				auto edgeScreen = worldToScreen(pick.worldPosition + camRight * m_Radius, viewProj, imagePos, imageSize);

				if (centerScreen && edgeScreen)
				{
					float screenRadius = glm::length(glm::vec2(edgeScreen->x - centerScreen->x, edgeScreen->y - centerScreen->y));
					ImGui::GetWindowDrawList()->AddCircle(*centerScreen, screenRadius, IM_COL32(255, 200, 80, 200), 32, 1.5f);
				}
			}

			bool mouseDown = state.viewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);

			if (mouseDown && pick.hit && !m_Dragging)
				beginStroke(entity, *meshAsset);

			if (m_Dragging && mouseDown && pick.hit)
			{
				auto& subMesh = meshAsset->getSubMeshesMutable()[0];
				applyBrush(subMesh, model, pick.worldPosition);
				Fufu::MeshUtils::recomputeNormals(subMesh);
				meshAsset->invalidateLODs(); // LOD0 changes every drag frame

				// The asset geometry has changed, not an ECS component: nothing
				// in Entity can detect this automatically, so we explicitly mark
				// the active scene dirty to force a GPU re-upload (this mesh's BLAS included).
				scene->markDirty();
				m_Renderer.resetAccumulation();
			}

			if (m_Dragging && !mouseDown)
				endStroke(state, entity);
		}

	private:
		enum class Mode { Push, Pull };

		void beginStroke(Fufu::Entity entity, Fufu::MeshAsset& asset)
		{
			m_Dragging = true;
			m_BeforeStroke = asset.getSubMeshes()[0];
			m_MeshPathAtStrokeStart = entity.getComponent<Fufu::MeshComponent>().meshPath;
		}

		void endStroke(EditorState& state, Fufu::Entity entity)
		{
			if (!m_Dragging) return;
			m_Dragging = false;

			if (!entity || !entity.isValid() || !entity.hasComponent<Fufu::MeshComponent>())
				return;

			auto meshAsset = getMeshAssetForEntity(entity);
			if (!meshAsset || meshAsset->getSubMeshCount() == 0 || !state.commandHistory)
				return;

			state.commandHistory->execute(std::make_unique<SculptStrokeCommand>(
				std::filesystem::path(m_MeshPathAtStrokeStart), std::size_t(0),
				m_BeforeStroke, meshAsset->getSubMeshes()[0]));
		}

		void applyBrush(Fufu::SubMesh& mesh, const glm::mat4& model, const glm::vec3& worldCenter)
		{
			glm::vec3 localCenter = glm::vec3(glm::inverse(model) * glm::vec4(worldCenter, 1.f));
			float sign = (m_Mode == Mode::Push) ? 1.f : -1.f;

			// Small per-frame factor: this brush applies continuously while
			// the mouse button is held (not once per click), so m_Strength is
			// deliberately dampened to remain controllable regardless of the
			// exact framerate (no deltaTime available in IEditorTool).
			const float perFrameFactor = 0.06f;

			for (Fufu::Vertex& v : mesh.vertices)
			{
				float dist = glm::length(v.position - localCenter);
				if (dist >= m_Radius) continue;

				float t = 1.f - (dist / m_Radius);
				float falloff = t * t * (3.f - 2.f * t); // smoothstep
				v.position += v.normal * (sign * m_Strength * falloff * perFrameFactor);
			}
		}

		Fufu::Renderer& m_Renderer;
		Mode m_Mode = Mode::Push;
		float m_Radius = 0.5f;
		float m_Strength = 0.5f;

		bool m_Dragging = false;
		Fufu::SubMesh m_BeforeStroke;
		std::string m_MeshPathAtStrokeStart;
	};
}
