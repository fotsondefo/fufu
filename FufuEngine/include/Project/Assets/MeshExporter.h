#pragma once

#include "MeshAsset.h"
#include <filesystem>

namespace Fufu
{
	class MeshExporter
	{
	public:
		// Writes a SubMesh in Wavefront OBJ format (v/vt/vn/f), readable as-is
		// by AssetManager::loadMesh (via Assimp) — no engine-specific treatment
		// needed on the import side.
		static bool writeObj(const std::filesystem::path& path, const SubMesh& mesh);
	};
}
