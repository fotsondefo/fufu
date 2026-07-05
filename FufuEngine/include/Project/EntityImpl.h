#pragma once
// EntityImpl.h est inclus a la fin de Scene.h,
// a ce moment Scene est completement defini donc m_Registry est accessible.

namespace Fufu {

	template<typename T, typename... Args>
	T& Entity::addComponent(Args&&... args)
	{
		FUFU_ASSERT(!hasComponent<T>(), "Entity already has this component");
		T& component = m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
		// Ajout/retrait de component est structurel et rare (jamais appelé dans
		// une boucle par-frame, contrairement à getComponent<T>() utilisé aussi
		// en lecture pure) : marquer la scène dirty ici attrape automatiquement
		// toute mutation de ce type sans devoir instrumenter chaque site
		// d'appel (Duplicate, ComponentAddCommand, création de primitives/
		// lumières, PrefabSerializer...).
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