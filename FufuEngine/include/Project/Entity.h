#pragma once

#include <entt/entt.hpp>

namespace Fufu 
{

	class Scene;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_Handle(handle), m_Scene(scene) {}

		template<typename T, typename... Args>
		T& addComponent(Args&&... args)
		{
			FUFU_ASSERT(!hasComponent<T>(), "Entity already has this component");
			return m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& getComponent()
		{
			FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
			return m_Scene->m_Registry.get<T>(m_Handle);
		}

		template<typename T>
		const T& getComponent() const
		{
			FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
			return m_Scene->m_Registry.get<T>(m_Handle);
		}

		template<typename T>
		bool hasComponent() const
		{
			return m_Scene->m_Registry.all_of<T>(m_Handle);
		}

		template<typename T>
		void removeComponent()
		{
			FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
			m_Scene->m_Registry.remove<T>(m_Handle);
		}

		bool isValid() const
		{
			return m_Scene && m_Scene->m_Registry.valid(m_Handle);
		}

		entt::entity getHandle() const { return m_Handle; }

		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }
		explicit operator bool() const { return isValid(); }

	private:
		entt::entity m_Handle = entt::null;
		Scene*       m_Scene = nullptr;

		friend class Scene;
	};

}