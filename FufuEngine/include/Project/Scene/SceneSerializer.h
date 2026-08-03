#pragma once

#include "Scene.h"

namespace Fufu
{
	class SceneSerializer
	{
	public:
		// Current .fufuscene file format version. To be incremented each time
		// the schema changes, with an entry added in migrateSceneJson()
		// (see SceneSerializer.cpp) to read older files without breaking them.
		static constexpr int k_CurrentVersion = 1;

		explicit SceneSerializer(Scene* scene) : m_Scene(scene) {}

		void serialize(const std::filesystem::path& path) const;
		bool deserialize(const std::filesystem::path& path);

	private:
		Scene* m_Scene = nullptr;
	};

}
