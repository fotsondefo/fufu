#pragma once

#include <Project/Entity.h>
#include <Project/Assets/MeshAsset.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <optional>

namespace FufuStudio
{
	struct MeshPickResult
	{
		bool hit = false;
		std::size_t faceIndex = 0;
		glm::vec3 worldPosition{};
	};

	// Pick approximatif en espace écran (utilisé par ModelingTool et
	// SculptTool) : projette chaque triangle, teste l'appartenance
	// barycentrique au point cliqué, garde le plus proche (clip.w comme proxy
	// de profondeur). Pas de vrai lancer de rayon 3D nécessaire pour un usage
	// éditeur sur des maillages de cette taille.
	MeshPickResult pickMesh(const Fufu::SubMesh& mesh, const glm::mat4& model,
		const glm::mat4& viewProj, glm::vec2 uv);

	// Projette un point monde vers l'écran (coordonnées ImGui, top-left origin).
	// Renvoie nullopt si le point est derrière la caméra.
	std::optional<ImVec2> worldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj,
		ImVec2 imagePos, ImVec2 imageSize);

	// Résout le MeshAsset référencé par le MeshComponent de l'entité (projet actif).
	std::shared_ptr<Fufu::MeshAsset> getMeshAssetForEntity(Fufu::Entity entity);
}
