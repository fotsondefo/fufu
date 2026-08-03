#pragma once

#include "GPUBuffers.h"
#include <entt/entt.hpp>
#include "Project/Assets/MeshAsset.h"
#include "Project/Components.h"
#include <vector>

namespace Fufu
{
	// Generates the GPU triangles of a GroomComponent (procedural hair cards)
	// from the surface of a MeshAsset, in LOCAL SPACE (no transform applied
	// here): the groom thus becomes its own small BLAS, instantiated like any
	// other mesh — see Renderer::uploadSceneData. The materialIndex of each
	// triangle is left at 0: the groom color is carried by the instance, not
	// by the shared geometry.
	class GroomGenerator
	{
	public:
		static void generate(const MeshAsset& mesh, const GroomComponent& groom,
			std::vector<GPUTriangle>& outTriangles);
	};
}
