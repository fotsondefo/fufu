#pragma once

#include "EditorState.h"
#include <Project/Assets/MeshAsset.h>

namespace FufuStudio
{
	// Écrit une primitive procédurale (Cube/Plane/Sphere/Cylinder, voir
	// Fufu::PrimitiveMeshes) sur disque une fois (réutilisable ensuite comme
	// n'importe quel mesh importé via Assimp) et crée une entité qui la
	// référence. Partagé par la Hierarchy et le Viewport, tous deux ayant un
	// menu "Add/Create Primitive".
	void createPrimitiveEntity(EditorState& state, const std::shared_ptr<Fufu::Scene>& scene,
		const std::string& name, const Fufu::SubMesh& mesh);
}
