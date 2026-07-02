#pragma once

#include "Scene/Scene.h"

namespace Fufu
{
	// Prefabs "snapshot" : sauvegarde figée d'une entité + ses descendants dans
	// un fichier .fufuprefab, et instanciation (copie) dans une scène. Pas de
	// synchronisation : modifier une instance ou le fichier prefab n'affecte
	// pas l'autre après l'instanciation.
	class PrefabSerializer
	{
	public:
		// Version courante du format .fufuprefab. Même logique d'évolution que
		// SceneSerializer::k_CurrentVersion (voir PrefabSerializer.cpp).
		static constexpr int k_CurrentVersion = 1;

		// Sérialise `root` et tous ses descendants (via ChildrenComponent) dans
		// un nouveau fichier. Le lien vers un éventuel parent HORS du sous-arbre
		// n'est pas conservé : l'entité redevient racine dans le prefab.
		static bool save(Scene* scene, Entity root, const std::filesystem::path& path);

		// Charge le fichier et crée une copie du sous-arbre dans `scene`.
		// Si `parent` est valide, la nouvelle racine lui est rattachée.
		// Retourne l'entité racine créée, ou une entité invalide en cas d'échec.
		static Entity instantiate(Scene* scene, const std::filesystem::path& path, Entity parent = {});
	};
}
