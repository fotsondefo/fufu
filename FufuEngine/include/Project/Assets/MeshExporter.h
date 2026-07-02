#pragma once

#include "MeshAsset.h"
#include <filesystem>

namespace Fufu
{
	class MeshExporter
	{
	public:
		// Écrit un SubMesh au format Wavefront OBJ (v/vt/vn/f), relisible tel
		// quel par AssetManager::loadMesh (via Assimp) — aucune spécificité
		// moteur nécessaire côté import.
		static bool writeObj(const std::filesystem::path& path, const SubMesh& mesh);
	};
}
