#include "depch.h"
#include "Project/Scene/SceneSerializer.h"
#include "Project/Scene/EntitySerialization.h"
#include "Project/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Fufu {

	// ----------------------------------------------------------------
	// Migration de schéma
	// ----------------------------------------------------------------
	// Appelée avec la version lue dans le fichier avant que deserialize()
	// n'interprète le JSON : transforme `root` en place jusqu'à ce qu'il
	// corresponde au schéma de SceneSerializer::k_CurrentVersion.
	// Exemple pour une future version 2 :
	//   if (fromVersion < 2) { /* renommer un champ, ajouter une valeur par défaut... */ }
	static void migrateSceneJson(json& root, int fromVersion)
	{
		(void)root;
		(void)fromVersion;
		// Rien à migrer : la seule version existante est k_CurrentVersion.
	}

	// ----------------------------------------------------------------
	// Serialize
	// ----------------------------------------------------------------

	void SceneSerializer::serialize(const std::filesystem::path& path) const
	{
		json root;
		root["scene"] = m_Scene->getName();
		root["version"] = SceneSerializer::k_CurrentVersion;
		root["entities"] = json::array();

		auto& reg = m_Scene->m_Registry;

		// Index stable (position dans le fichier) indépendant des handles entt,
		// qui peuvent différer d'un chargement à l'autre.
		std::unordered_map<entt::entity, int> indexMap;
		int nextIndex = 0;
		reg.each([&](entt::entity handle) { indexMap[handle] = nextIndex++; });

		reg.each([&](entt::entity handle) {
			root["entities"].push_back(serializeEntityToJson(handle, reg, indexMap));
		});

		std::ofstream file(path);
		FUFU_ASSERT(file.is_open(), "Failed to open file for writing: {}", path.string());
		file << root.dump(4); // indentation 4 espaces
		FUFU_INFO("Scene '{}' serialized to '{}'", m_Scene->getName(), path.string());
	}

	// ----------------------------------------------------------------
	// Deserialize
	// ----------------------------------------------------------------

	bool SceneSerializer::deserialize(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			FUFU_ERROR("Failed to open scene file: {}", path.string());
			return false;
		}

		json root;
		try { root = json::parse(file); }
		catch (const json::parse_error& e)
		{
			FUFU_ERROR("JSON parse error in '{}': {}", path.string(), e.what());
			return false;
		}

		int fileVersion = root.value("version", 1);
		if (fileVersion > SceneSerializer::k_CurrentVersion)
		{
			FUFU_WARN("Scene '{}' was saved with a newer format (v{} > v{} supported) — loading best-effort",
				path.string(), fileVersion, SceneSerializer::k_CurrentVersion);
		}
		else if (fileVersion < SceneSerializer::k_CurrentVersion)
		{
			migrateSceneJson(root, fileVersion);
		}

		m_Scene->setName(root.at("scene").get<std::string>());

		// id (tel qu'écrit dans le fichier) -> entité nouvellement créée.
		std::unordered_map<int, Entity> idToEntity;

		for (const auto& j : root.at("entities"))
		{
			Entity entity = createEntityFromJson(m_Scene, j);
			idToEntity[j.at("id").get<int>()] = entity;
		}

		// Deuxième passe pour reconstruire la hiérarchie (toutes les entités
		// doivent exister avant de résoudre les liens parent/enfant).
		for (const auto& j : root.at("entities"))
		{
			if (!j.contains("parent"))
				continue;

			auto childIt = idToEntity.find(j.at("id").get<int>());
			auto parentIt = idToEntity.find(j.at("parent").get<int>());

			if (childIt != idToEntity.end() && parentIt != idToEntity.end())
				m_Scene->setParent(childIt->second, parentIt->second);
		}

		FUFU_INFO("Scene '{}' deserialized from '{}'", m_Scene->getName(), path.string());
		return true;
	}

}
