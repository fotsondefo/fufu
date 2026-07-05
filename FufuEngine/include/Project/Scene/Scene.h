#pragma once

#include <entt/entt.hpp>
#include "Project/Entity.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/EnvironmentSettings.h"

namespace Fufu
{

	class Scene
	{
	public:
		Scene() = default;
		explicit Scene(const std::string& name) : m_Name(name) {}
		~Scene() = default;

		// Entity creation / destruction
		Entity createEntity(const std::string& tag = "Entity");
		void   destroyEntity(Entity entity);

		// Hierarchy
		void setParent(Entity child, Entity parent);
		void removeParent(Entity child);

		// Interate on the component of a type 
		template<typename... Components, typename Func>
		void each(Func&& func)
		{
			auto view = m_Registry.view<Components...>();
			view.each(std::forward<Func>(func));
		}

		Entity getPrimaryCamera();

		const std::string& getName() const { return m_Name; }
		void               setName(const std::string& name) { m_Name = name; }

		entt::registry& getRegistry() { return m_Registry; }

		// Paramètres de rendu propres à CETTE scène (technique, AA, exposition...) :
		// sauvegardés/chargés avec le fichier .fufuscene (voir SceneSerializer),
		// et poussés dans le Renderer lorsque la scène devient active (voir
		// EditorState::syncToActiveScene côté FufuStudio).
		RenderSettings&       getRenderSettings()       { return m_RenderSettings; }
		const RenderSettings& getRenderSettings() const { return m_RenderSettings; }

		// Environnement (skybox) de cette scène : lu directement par Renderer
		// à chaque frame (pas de mirroring comme RenderSettings, puisque
		// Renderer::renderScene reçoit déjà la Scene en paramètre).
		EnvironmentSettings&       getEnvironment()       { return m_Environment; }
		const EnvironmentSettings& getEnvironment() const { return m_Environment; }

		// Compteur bump é à chaque mutation de contenu qui nécessite un
		// re-upload GPU (structure de l'ECS via createEntity/destroyEntity/
		// setParent/removeParent et addComponent/removeComponent — voir
		// EntityImpl.h —, ou explicitement par les commandes qui éditent un
		// component/mesh en place). Comparé par Renderer::sceneNeedsUpdate à
		// une valeur mise en cache pour éviter de reconstruire toute la
		// géométrie GPU (BVH inclus) à chaque frame alors que rien n'a changé.
		void     markDirty() { ++m_Version; }
		uint32_t getVersion() const { return m_Version; }

	private:
		entt::registry m_Registry;
		std::string    m_Name = "Untitled";
		RenderSettings m_RenderSettings;
		EnvironmentSettings m_Environment;
		uint32_t m_Version = 0;

		friend class Entity;
		friend class SceneSerializer;
	};

}

// Inclusion differee : a ce stade Scene est completement defini,
// EntityImpl.h peut donc implementer les methodes template d'Entity.
#include "Project/EntityImpl.h"