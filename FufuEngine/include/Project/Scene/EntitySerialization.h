#pragma once

#include "Scene.h"
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace Fufu
{
	// Sérialisation par-entité, partagée entre SceneSerializer (scène entière)
	// et PrefabSerializer (sous-arbre). L'id écrit dans le JSON n'est PAS le
	// handle entt brut (celui-ci peut être recyclé différemment à chaque
	// chargement) : c'est un index stable fourni par l'appelant via indexMap,
	// résolu vers un entt::entity fraîchement créé au chargement.

	nlohmann::json serializeEntityToJson(entt::entity handle, entt::registry& reg,
		const std::unordered_map<entt::entity, int>& indexMap);

	// Crée une entité et ses components à partir de son JSON (tag, transform,
	// mesh, material, camera). Ne résout PAS le parent : l'appelant doit faire
	// une seconde passe une fois toutes les entités créées, via l'id mapping.
	Entity createEntityFromJson(Scene* scene, const nlohmann::json& j);
}
