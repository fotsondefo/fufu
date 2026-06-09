#pragma once

#include "Scene.h"

namespace Fufu 
{
	class SceneSerializer
	{
	public:
		explicit SceneSerializer(Scene* scene) : m_Scene(scene) {}

		void serialize(const std::filesystem::path& path) const;
		bool deserialize(const std::filesystem::path& path);

	private:
		Scene* m_Scene = nullptr;
	};

}