#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	class MeshUtils
	{
	public:
		// Recomputes per-vertex normals by averaging the geometric normals of
		// the faces that share each vertex (area-weighted, since not normalized
		// before accumulation). Vertices not shared between faces (e.g. our
		// hard-normal primitives like Cube) retain a "flat" look by construction
		// — no topology welding here.
		static void recomputeNormals(SubMesh& mesh);
	};
}
