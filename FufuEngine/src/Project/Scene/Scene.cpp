#include "depch.h"

#include "Project/Scene/Scene.h"
#include "Project/Components.h"

namespace Fufu 
{
	Entity Scene::createEntity(const std::string& tag)
	{
		Entity entity(m_Registry.create(), this);
		entity.addComponent<TagComponent>(tag);
		entity.addComponent<TransformComponent>();
		markDirty();
		return entity;
	}

	void Scene::destroyEntity(Entity entity)
	{
		// D�tacher des parents/enfants avant destruction
		removeParent(entity);

		if (entity.hasComponent<ChildrenComponent>())
		{
			// D�tacher tous les enfants
			auto& children = entity.getComponent<ChildrenComponent>().children;
			for (entt::entity child : children)
			{
				if (m_Registry.valid(child))
					m_Registry.remove<ParentComponent>(child);
			}
		}

		m_Registry.destroy(entity.getHandle());
		markDirty();
	}

	void Scene::setParent(Entity child, Entity parent)
	{
		// Retirer l'ancien parent si besoin
		removeParent(child);

		// Assigner le nouveau parent
		if (!child.hasComponent<ParentComponent>())
			child.addComponent<ParentComponent>();
		child.getComponent<ParentComponent>().parent = parent.getHandle();

		// Enregistrer l'enfant chez le parent
		if (!parent.hasComponent<ChildrenComponent>())
			parent.addComponent<ChildrenComponent>();
		parent.getComponent<ChildrenComponent>().addChild(child.getHandle());

		markDirty();
	}

	void Scene::removeParent(Entity child)
	{
		if (!child.hasComponent<ParentComponent>())
			return;

		entt::entity parentHandle = child.getComponent<ParentComponent>().parent;

		if (m_Registry.valid(parentHandle) && m_Registry.all_of<ChildrenComponent>(parentHandle))
			m_Registry.get<ChildrenComponent>(parentHandle).removeChild(child.getHandle());

		child.removeComponent<ParentComponent>();
		markDirty();
	}

	Entity Scene::getPrimaryCamera()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto e : view)
		{
			if (view.get<CameraComponent>(e).primary)
				return Entity(e, this);
		}
		return Entity{}; // Aucune cam�ra primaire
	}

}