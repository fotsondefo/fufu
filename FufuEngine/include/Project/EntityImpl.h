#pragma once
// EntityImpl.h is included at the end of Scene.h,
// at which point Scene is fully defined so m_Registry is accessible.

namespace Fufu {

	template<typename T, typename... Args>
	T& Entity::addComponent(Args&&... args)
	{
		FUFU_ASSERT(!hasComponent<T>(), "Entity already has this component");
		T& component = m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
		// Adding/removing a component is structural and rare (never called in a
		// per-frame loop, unlike getComponent<T>() which is also used for pure
		// reads): marking the scene dirty here automatically catches all mutations
		// of this type without having to instrument every call site (Duplicate,
		// ComponentAddCommand, primitive/light creation, PrefabSerializer...).
		m_Scene->markDirty();
		return component;
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
		m_Scene->markDirty();
	}

}