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
		// clip.w du point touché (voir pickMesh) : comparable entre plusieurs
		// meshes/entités tant qu'ils partagent le même viewProj, ce qui permet
		// à pickEntity() de garder le hit le plus proche tous meshes confondus.
		float depth = std::numeric_limits<float>::max();
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

	// Sélection d'objet dans le Viewport : cherche, parmi toutes les entités
	// (Transform+Mesh) de la scène, celle dont un triangle est le plus proche
	// sous le point cliqué. Contrairement à pickMesh (une seule entité, déjà
	// sélectionnée, utilisé par Model/Sculpt pour du pick de face), celui-ci
	// parcourt toute la scène pour déterminer QUELLE entité cliquer sélectionne.
	std::optional<Fufu::Entity> pickEntity(Fufu::Scene& scene, const glm::mat4& viewProj, glm::vec2 uv);
}
