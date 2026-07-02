#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	// Générateurs de maillages procéduraux de base, utilisés par l'action
	// "Create Primitive" de l'éditeur. Toutes les primitives sont centrées à
	// l'origine, taille ~2 unités (convention type Blender : cube -1..1).
	class PrimitiveMeshes
	{
	public:
		static SubMesh makeCube();
		static SubMesh makePlane();
		static SubMesh makeSphere(int rings = 16, int segments = 24);
		static SubMesh makeCylinder(int segments = 24);
	};
}
