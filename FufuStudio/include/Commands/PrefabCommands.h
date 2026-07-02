#pragma once

#include "ICommand.h"
#include <Project/Scene/Scene.h>
#include <Project/Components.h>
#include <Project/PrefabSerializer.h>
#include <filesystem>
#include <memory>

namespace FufuStudio
{
	// Instancie un prefab (snapshot statique, voir PrefabSerializer) dans la
	// scène. redo() ré-instancie depuis le fichier à chaque fois (même
	// logique que EntityCreateCommand) ; undo() détruit tout le sous-arbre créé.
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

			// Copie : Scene::destroyEntity va modifier la ChildrenComponent du
			// parent, pas celle qu'on itère (les enfants n'ont pas de lien entre eux).
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
