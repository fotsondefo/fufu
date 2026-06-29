#pragma once

#include <entt/entt.hpp>
#include "Project/Entity.h"

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

	private:
		entt::registry m_Registry;
		std::string    m_Name = "Untitled";

		friend class Entity;
		friend class SceneSerializer;
	};

}