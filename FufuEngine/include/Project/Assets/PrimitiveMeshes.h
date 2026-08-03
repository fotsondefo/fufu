#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	// Basic procedural mesh generators, used by the editor's "Create Primitive"
	// action. All primitives are centered at the origin, size ~2 units
	// (Blender-style convention: cube -1..1).
	class PrimitiveMeshes
	{
	public:
		static SubMesh makeCube();
		static SubMesh makePlane();
		static SubMesh makeSphere(int rings = 16, int segments = 24);
		static SubMesh makeCylinder(int segments = 24);
	};
}
