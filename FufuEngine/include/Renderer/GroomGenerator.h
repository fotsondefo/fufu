#pragma once

#include "GPUBuffers.h"
#include <entt/entt.hpp>
#include "Project/Assets/MeshAsset.h"
#include "Project/Components.h"
#include <vector>

namespace Fufu
{
	// Génère les triangles GPU d'un GroomComponent (hair cards procéduraux)
	// à partir de la surface d'un MeshAsset, et les ajoute à outTriangles.
	// Appelé depuis Renderer::uploadSceneData — voir GroomComponent pour le
	// détail des limitations (rubans plats, pas de mèches courbes rendues
	// individuellement, un seul sous-maillage source).
	class GroomGenerator
	{
	public:
		static void generate(const MeshAsset& mesh, const GroomComponent& groom,
			const glm::mat4& model, const glm::mat3& normalMatrix,
			int materialIndex, std::vector<GPUTriangle>& outTriangles);
	};
}
