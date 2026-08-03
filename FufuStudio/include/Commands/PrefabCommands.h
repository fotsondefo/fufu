#pragma once

#include "ICommand.h"
#include <Project/Scene/Scene.h>
#include <Project/Components.h>
#include <Project/PrefabSerializer.h>
#include <filesystem>
#include <memory>

namespace FufuStudio
{
	// Instantiates a prefab (static snapshot, see PrefabSerializer) into the
	// scene. redo() re-instantiates from the file each time (same logic as
	// EntityCreateCommand); undo() destroys the entire created sub-tree.
	class PrefabInstantiateCommand : public ICommand
	{
	public:
		PrefabInstantiateCommand(std::shared_ptr<Fufu::Scene> scene, std::filesystem::path path,
			Fufu::Entity parent = {})
			: m_Scene(std::move(scene)), m_Path(std::move(path)), m_Parent(parent) {}

		void execute() override
		{
			m_Root = Fufu::PrefabSerializer::instantiate(m_Scene.get(), m_Path, m_Parent);
		}

		void undo() override
		{
			destroyRecursive(m_Root);
			m_Root = {};
		}

		const char* getName() const override { return "Instantiate Prefab"; }

		Fufu::Entity getEntity() const { return m_Root; }

	private:
		void destroyRecursive(Fufu::Entity entity)
		{
			if (!entity.isValid())
				return;

			// Copy: Scene::destroyEntity will modify the ChildrenComponent of the
			// parent, not the one we are iterating (children have no link to each other).
			std::vector<Fufu::Entity> children;
			if (entity.hasComponent<Fufu::ChildrenComponent>())
			{
				for (entt::entity child : entity.getComponent<Fufu::ChildrenComponent>().children)
					children.emplace_back(child, m_Scene.get());
			}

			for (Fufu::Entity child : children)
				destroyRecursive(child);

			m_Scene->destroyEntity(entity);
		}

		std::shared_ptr<Fufu::Scene> m_Scene;
		std::filesystem::path m_Path;
		Fufu::Entity m_Parent;
		Fufu::Entity m_Root;
	};
}
