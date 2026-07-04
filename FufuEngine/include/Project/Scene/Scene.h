#pragma once

#include <entt/entt.hpp>
#include "Project/Entity.h"
#include "Renderer/RenderSettings.h"

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

	private:
		entt::registry m_Registry;
		std::string    m_Name = "Untitled";
		RenderSettings m_RenderSettings;

		friend class Entity;
		friend class SceneSerializer;
	};

}

// Inclusion differee : a ce stade Scene est completement defini,
// EntityImpl.h peut donc implementer les methodes template d'Entity.
#include "Project/EntityImpl.h"