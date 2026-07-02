#pragma once

#include "Scene.h"

namespace Fufu
{
	class SceneSerializer
	{
	public:
		// Version courante du format de fichier .fufuscene. À incrémenter
		// chaque fois que le schéma change, avec une entrée ajoutée dans
		// migrateSceneJson() (voir SceneSerializer.cpp) pour lire les anciens
		// fichiers sans les casser.
		static constexpr int k_CurrentVersion = 1;

		explicit SceneSerializer(Scene* scene) : m_Scene(scene) {}

		void serialize(const std::filesystem::path& path) const;
		bool deserialize(const std::filesystem::path& path);

	private:
		Scene* m_Scene = nullptr;
	};

}
