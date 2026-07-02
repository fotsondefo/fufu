#include "depch.h"
#include "Project/PrefabSerializer.h"
#include "Project/Scene/EntitySerialization.h"
#include "Project/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Fufu
{
	// Parcourt le sous-arbre (racine incluse) en profondeur, dans un ordre
	// déterministe : la racine est toujours en première position.
	static void collectSubtree(Scene* scene, entt::entity handle, std::vector<entt::entity>& out)
	{
		Entity entity(handle, scene);
		if (!entity.isValid())
			return;

		out.push_back(handle);

		if (entity.hasComponent<ChildrenComponent>())
		{
			for (entt::entity childHandle : entity.getComponent<ChildrenComponent>().children)
				collectSubtree(scene, childHandle, out);
		}
	}

	bool PrefabSerializer::save(Scene* scene, Entity root, const std::filesystem::path& path)
	{
		if (!scene || !root || !root.isValid())
			return false;

		std::vector<entt::entity> subtree;
		collectSubtree(scene, root.getHandle(), subtree);

		std::unordered_map<entt::entity, int> indexMap;
		for (std::size_t i = 0; i < subtree.size(); ++i)
			indexMap[subtree[i]] = static_cast<int>(i);

		json doc;
		doc["prefab"] = true;
		doc["version"] = PrefabSerializer::k_CurrentVersion;
		doc["rootId"] = 0; // la racine est toujours le premier élément (voir collectSubtree)
		doc["entities"] = json::array();

		auto& reg = scene->getRegistry();
		for (entt::entity handle : subtree)
			doc["entities"].push_back(serializeEntityToJson(handle, reg, indexMap));

		std::ofstream file(path);
		if (!file.is_open())
		{
			FUFU_ERROR("Failed to open file for writing: {}", path.string());
			return false;
		}

		file << doc.dump(4);
		FUFU_INFO("Prefab '{}' saved to '{}'", root.getComponent<TagComponent>().tag, path.string());
		return true;
	}

	Entity PrefabSerializer::instantiate(Scene* scene, const std::filesystem::path& path, Entity parent)
	{
		if (!scene)
			return {};

		std::ifstream file(path);
		if (!file.is_open())
		{
			FUFU_ERROR("Failed to open prefab file: {}", path.string());
			return {};
		}

		json doc;
		try { doc = json::parse(file); }
		catch (const json::parse_error& e)
		{
			FUFU_ERROR("JSON parse error in '{}': {}", path.string(), e.what());
			return {};
		}

		int fileVersion = doc.value("version", 1);
		if (fileVersion > PrefabSerializer::k_CurrentVersion)
		{
			FUFU_WARN("Prefab '{}' was saved with a newer format (v{} > v{} supported) — loading best-effort",
				path.string(), fileVersion, PrefabSerializer::k_CurrentVersion);
		}
		// Une seule version existante pour l'instant : pas de migration nécessaire.

		std::unordered_map<int, Entity> idToEntity;
		for (const auto& j : doc.at("entities"))
		{
			Entity entity = createEntityFromJson(scene, j);
			idToEntity[j.at("id").get<int>()] = entity;
		}

		for (const auto& j : doc.at("entities"))
		{
			if (!j.contains("parent"))
				continue;

			auto childIt = idToEntity.find(j.at("id").get<int>());
			auto parentIt = idToEntity.find(j.at("parent").get<int>());

			if (childIt != idToEntity.end() && parentIt != idToEntity.end())
				scene->setParent(childIt->second, parentIt->second);
		}

		int rootId = doc.value("rootId", 0);
		auto rootIt = idToEntity.find(rootId);
		if (rootIt == idToEntity.end())
		{
			FUFU_ERROR("Prefab '{}' has no valid root entity", path.string());
			return {};
		}

		Entity rootEntity = rootIt->second;
		rootEntity.addComponent<PrefabInstanceComponent>(path.string());

		if (parent && parent.isValid())
			scene->setParent(rootEntity, parent);

		FUFU_INFO("Prefab instantiated from '{}'", path.string());
		return rootEntity;
	}
}
