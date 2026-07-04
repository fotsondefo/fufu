#pragma once

#include "GPUBuffers.h"
#include <entt/entt.hpp>
#include "Project/Assets/MeshAsset.h"
#include "Project/Components.h"
#include <vector>

namespace Fufu
{
	// Génère les triangles GPU d'un GroomComponent (hair cards procéduraux) à
	// partir de la surface d'un MeshAsset, en ESPACE LOCAL (pas de transform
	// appliqué ici) : le groom devient ainsi son propre petit BLAS, instancié
	// comme n'importe quel mesh — voir Renderer::uploadSceneData. Le
	// materialIndex de chaque triangle est laissé à 0 : la couleur du groom
	// est portée par l'instance, pas par la géométrie partagée.
	class GroomGenerator
	{
	public:
		static void generate(const MeshAsset& mesh, const GroomComponent& groom,
			std::vector<GPUTriangle>& outTriangles);
	};
}
