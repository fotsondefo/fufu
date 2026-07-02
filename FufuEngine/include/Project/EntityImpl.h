#pragma once
// EntityImpl.h est inclus a la fin de Scene.h,
// a ce moment Scene est completement defini donc m_Registry est accessible.

namespace Fufu {

	template<typename T, typename... Args>
	T& Entity::addComponent(Args&&... args)
	{
		FUFU_ASSERT(!hasComponent<T>(), "Entity already has this component");
		return m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Entity::getComponent()
	{
		FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
		return m_Scene->m_Registry.get<T>(m_Handle);
	}

	template<typename T>
	const T& Entity::getComponent() const
	{
		FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
		return m_Scene->m_Registry.get<T>(m_Handle);
	}

	template<typename T>
	bool Entity::hasComponent() const
	{
		return m_Scene->m_Registry.all_of<T>(m_Handle);
	}

	template<typename T>
	void Entity::removeComponent()
	{
		FUFU_ASSERT(hasComponent<T>(), "Entity does not have this component");
		m_Scene->m_Registry.remove<T>(m_Handle);
	}

}