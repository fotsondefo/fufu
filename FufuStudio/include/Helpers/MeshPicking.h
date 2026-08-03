#pragma once

#include <Project/Entity.h>
#include <Project/Scene/Scene.h>
#include <Project/Assets/MeshAsset.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <limits>
#include <optional>

namespace FufuStudio
{
	struct MeshPickResult
	{
		bool hit = false;
		std::size_t faceIndex = 0;
		glm::vec3 worldPosition{};
		// clip.w of the hit point (see pickMesh): comparable across multiple
		// meshes/entities as long as they share the same viewProj, which allows
		// pickEntity() to keep the closest hit across all meshes.
		float depth = std::numeric_limits<float>::max();
	};

	// Approximate screen-space pick (used by ModelingTool and
	// SculptTool): projects each triangle, tests barycentric membership
	// at the clicked point, keeps the closest one (clip.w as depth proxy).
	// No true 3D ray-casting needed for editor use on meshes of this size.
	MeshPickResult pickMesh(const Fufu::SubMesh& mesh, const glm::mat4& model,
		const glm::mat4& viewProj, glm::vec2 uv);

	// Projects a world point to screen (ImGui coordinates, top-left origin).
	// Returns nullopt if the point is behind the camera.
	std::optional<ImVec2> worldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj,
		ImVec2 imagePos, ImVec2 imageSize);

	// Resolves the MeshAsset referenced by the entity's MeshComponent (active project).
	std::shared_ptr<Fufu::MeshAsset> getMeshAssetForEntity(Fufu::Entity entity);

	// Object selection in the Viewport: searches, among all entities
	// (Transform+Mesh) in the scene, the one whose triangle is closest
	// under the clicked point. Unlike pickMesh (a single already-selected entity,
	// used by Model/Sculpt for face picking), this one traverses the entire
	// scene to determine WHICH entity a click selects.
	std::optional<Fufu::Entity> pickEntity(Fufu::Scene& scene, const glm::mat4& viewProj, glm::vec2 uv);
}
