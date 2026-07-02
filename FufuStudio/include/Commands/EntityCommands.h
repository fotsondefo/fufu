#pragma once

#include "ICommand.h"
#include <Project/Scene/Scene.h>
#include <Project/Components.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace FufuStudio
{
	// Cr�e une entit� (vide, enfant, ou avec un setup custom comme une cam�ra).
	// redo() recr�e une NOUVELLE entit� (nouveau handle entt) � chaque fois ;
	// c'est correct car la commande garde toujours la r�f�rence � jour via getEntity().
	class EntityCreateCommand : public ICommand
	{
	public:
		using SetupFn = std::function<void(Fufu::Entity)>;

		EntityCreateCommand(std::shared_ptr<Fufu::Scene> scene, std::string tag,
			Fufu::Entity parent = {}, SetupFn setup = nullptr)
			: m_Scene(std::move(scene)), m_Tag(std::move(tag)), m_Parent(parent), m_Setup(std::move(setup)) {}

		void execute() override
		{
			m_Entity = m_Scene->createEntity(m_Tag);

			if (m_Parent && m_Parent.isValid())
				m_Scene->setParent(m_Entity, m_Parent);
			
			if (m_Setup)
				m_Setup(m_Entity);
		}

		void undo() override
		{
			if (m_Entity && m_Entity.isValid())
				m_Scene->destroyEntity(m_Entity);
			
			m_Entity = {};
		}

		const char* getName() const override { return "Create Entity"; }

		Fufu::Entity getEntity() const { return m_Entity; }

	private:
		std::shared_ptr<Fufu::Scene> m_Scene;
		std::string m_Tag;
		Fufu::Entity m_Parent;
		Fufu::Entity m_Entity;
		SetupFn m_Setup;
	};

	// Duplique une entit� existante (copie Tag + Transform, comportement
	// identique � celui qu'avait la Hierarchy avant l'introduction des commandes).
	class EntityDuplicateCommand : public ICommand
	{
	public:
		EntityDuplicateCommand(std::shared_ptr<Fufu::Scene> scene, Fufu::Entity source)
			: m_Scene(std::move(scene)), m_Source(source) {}

		void execute() override
		{
			std::string tag = m_Source.getComponent<Fufu::TagComponent>().tag + " (copy)";
			m_Duplicate = m_Scene->createEntity(tag);
			
			m_Duplicate.getComponent<Fufu::TransformComponent>() = m_Source.getComponent<Fufu::TransformComponent>();
		}

		void undo() override
		{
			if (m_Duplicate && m_Duplicate.isValid())
				m_Scene->destroyEntity(m_Duplicate);
			
			m_Duplicate = {};
		}

		const char* getName() const override { return "Duplicate Entity"; }

		Fufu::Entity getEntity() const { return m_Duplicate; }

	private:
		std::shared_ptr<Fufu::Scene> m_Scene;
		Fufu::Entity m_Source;
		Fufu::Entity m_Duplicate;
	};

	// D�truit une entit� et garde un snapshot complet (components + hi�rarchie)
	// pour pouvoir la restaurer � l'identique au undo.
	class EntityDestroyCommand : public ICommand
	{
	public:
		EntityDestroyCommand(std::shared_ptr<Fufu::Scene> scene, Fufu::Entity entity)
			: m_Scene(std::move(scene)), m_Entity(entity) {}

		void execute() override
		{
			snapshot();
			m_Scene->destroyEntity(m_Entity);
			m_Entity = {};
		}

		void undo() override
		{
			m_Entity = m_Scene->createEntity(m_Tag);
			m_Entity.getComponent<Fufu::TransformComponent>() = m_Transform;

			if (m_HasMesh)     m_Entity.addComponent<Fufu::MeshComponent>(m_Mesh);
			if (m_HasMaterial) m_Entity.addComponent<Fufu::MaterialComponent>(m_Material);
			if (m_HasCamera)   m_Entity.addComponent<Fufu::CameraComponent>(m_Camera);

			if (m_Parent && m_Parent.isValid())
				m_Scene->setParent(m_Entity, m_Parent);

			// Ré-attacher les enfants qui pointaient vers l'entité détruite
			for (Fufu::Entity& child : m_Children)
			{
				if (child.isValid())
					m_Scene->setParent(child, m_Entity);
			}
		}

		const char* getName() const override { return "Delete Entity"; }

		Fufu::Entity getEntity() const { return m_Entity; }

	private:
		void snapshot()
		{
			m_Tag = m_Entity.getComponent<Fufu::TagComponent>().tag;
			m_Transform = m_Entity.getComponent<Fufu::TransformComponent>();

			m_HasMesh = m_Entity.hasComponent<Fufu::MeshComponent>();
			if (m_HasMesh) m_Mesh = m_Entity.getComponent<Fufu::MeshComponent>();

			m_HasMaterial = m_Entity.hasComponent<Fufu::MaterialComponent>();
			if (m_HasMaterial) m_Material = m_Entity.getComponent<Fufu::MaterialComponent>();

			m_HasCamera = m_Entity.hasComponent<Fufu::CameraComponent>();
			if (m_HasCamera) m_Camera = m_Entity.getComponent<Fufu::CameraComponent>();

			m_Parent = {};
			if (m_Entity.hasComponent<Fufu::ParentComponent>())
			{
				entt::entity parentHandle = m_Entity.getComponent<Fufu::ParentComponent>().parent;
				m_Parent = Fufu::Entity(parentHandle, m_Scene.get());
			}

			m_Children.clear();
			if (m_Entity.hasComponent<Fufu::ChildrenComponent>())
			{
				for (entt::entity child : m_Entity.getComponent<Fufu::ChildrenComponent>().children)
					m_Children.emplace_back(child, m_Scene.get());
			}
		}

		std::shared_ptr<Fufu::Scene> m_Scene;
		Fufu::Entity m_Entity;

		std::string m_Tag;
		Fufu::TransformComponent m_Transform;

		bool m_HasMesh = false;
		Fufu::MeshComponent m_Mesh;

		bool m_HasMaterial = false;
		Fufu::MaterialComponent m_Material;

		bool m_HasCamera = false;
		Fufu::CameraComponent m_Camera;

		Fufu::Entity m_Parent;
		std::vector<Fufu::Entity> m_Children;
	};

	// Change le parent d'une entit� (drag & drop dans la Hierarchy).
	class EntityReparentCommand : public ICommand
	{
	public:
		EntityReparentCommand(std::shared_ptr<Fufu::Scene> scene, Fufu::Entity child, Fufu::Entity newParent)
			: m_Scene(std::move(scene)), m_Child(child), m_NewParent(newParent)
		{
			if (child.hasComponent<Fufu::ParentComponent>())
			{
				entt::entity parentHandle = child.getComponent<Fufu::ParentComponent>().parent;
				m_OldParent = Fufu::Entity(parentHandle, m_Scene.get());
			}
		}

		void execute() override { m_Scene->setParent(m_Child, m_NewParent); }

		void undo() override
		{
			if (m_OldParent && m_OldParent.isValid())
				m_Scene->setParent(m_Child, m_OldParent);
			else
				m_Scene->removeParent(m_Child);
		}

		const char* getName() const override { return "Reparent Entity"; }

	private:
		std::shared_ptr<Fufu::Scene> m_Scene;
		Fufu::Entity m_Child;
		Fufu::Entity m_NewParent;
		Fufu::Entity m_OldParent;
	};
}
