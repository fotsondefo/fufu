#pragma once

#include "Scene.h"
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace Fufu
{
	// Per-entity serialization, shared between SceneSerializer (full scene)
	// and PrefabSerializer (sub-tree). The id written in JSON is NOT the raw
	// entt handle (which can be recycled differently on each load): it is a
	// stable index provided by the caller via indexMap, resolved to a freshly
	// created entt::entity at load time.

	nlohmann::json serializeEntityToJson(entt::entity handle, entt::registry& reg,
		const std::unordered_map<entt::entity, int>& indexMap);

	// Creates an entity and its components from its JSON (tag, transform,
	// mesh, material, camera). Does NOT resolve the parent: the caller must
	// perform a second pass once all entities are created, via the id mapping.
	Entity createEntityFromJson(Scene* scene, const nlohmann::json& j);
}
