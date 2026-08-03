#pragma once

#include "Scene/Scene.h"

namespace Fufu
{
	// "Snapshot" prefabs: frozen save of an entity + its descendants in a
	// .fufuprefab file, and instantiation (copy) into a scene. No
	// synchronization: modifying an instance or the prefab file does not
	// affect the other after instantiation.
	class PrefabSerializer
	{
	public:
		// Current .fufuprefab format version. Same evolution logic as
		// SceneSerializer::k_CurrentVersion (see PrefabSerializer.cpp).
		static constexpr int k_CurrentVersion = 1;

		// Serializes `root` and all its descendants (via ChildrenComponent) into
		// a new file. The link to a potential parent OUTSIDE the sub-tree is not
		// preserved: the entity becomes the root again in the prefab.
		static bool save(Scene* scene, Entity root, const std::filesystem::path& path);

		// Loads the file and creates a copy of the sub-tree in `scene`.
		// If `parent` is valid, the new root is attached to it.
		// Returns the created root entity, or an invalid entity on failure.
		static Entity instantiate(Scene* scene, const std::filesystem::path& path, Entity parent = {});
	};
}
