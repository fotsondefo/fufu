#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	class MeshSimplifier
	{
	public:
		// Simplification by vertex clustering on a grid of cells of size
		// `cellSize` (same units as mesh positions): vertices falling in the
		// same cell merge into one (averaged position/normal/uv), degenerate
		// triangles are removed. Intentionally simple (no quadric error metric)
		// — fast, topology not guaranteed optimal, but a "good enough" LOD at
		// low implementation cost. See MeshAsset::getLODSubMeshes.
		static SubMesh simplify(const SubMesh& source, float cellSize);
	};
}
