#pragma once

#include "EditorState.h"
#include <Project/Assets/MeshAsset.h>

namespace FufuStudio
{
	// Writes a procedural primitive (Cube/Plane/Sphere/Cylinder, see
	// Fufu::PrimitiveMeshes) to disk once (reusable afterwards like any mesh
	// imported via Assimp) and creates an entity that references it.
	// Shared by the Hierarchy and the Viewport, both of which have an
	// "Add/Create Primitive" menu.
	void createPrimitiveEntity(EditorState& state, const std::shared_ptr<Fufu::Scene>& scene,
		const std::string& name, const Fufu::SubMesh& mesh);
}
