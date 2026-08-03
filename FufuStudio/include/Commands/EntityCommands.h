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
	// Creates an entity (empty, child, or with a custom setup such as a camera).
	// redo() re-creates a NEW entity (new entt handle) each time;
	// this is correct because the command always keeps the reference up to date via getEntity().
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

	// Duplicates an existing entity: Transform + all optional components it carries
	// (Mesh, Material, Camera, Groom, Light, prefab link), and places it as a
	// sibling under the same parent as the source if there was one.
	// Does NOT duplicate children (sub-tree) — only the entity itself.
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

			if (m_Source.hasComponent<Fufu::MeshComponent>())
				m_Duplicate.addComponent<Fufu::MeshComponent>(m_Source.getComponent<Fufu::MeshComponent>());

			if (m_Source.hasComponent<Fufu::MaterialComponent>())
				m_Duplicate.addComponent<Fufu::MaterialComponent>(m_Source.getComponent<Fufu::MaterialComponent>());

			if (m_Source.hasComponent<Fufu::CameraComponent>())
			{
				// Only one "primary" camera at a time: the copy is never
				// primary by default, to avoid ending up with two
				// primary cameras after a duplicate.
				Fufu::CameraComponent cam = m_Source.getComponent<Fufu::CameraComponent>();
				cam.primary = false;
				m_Duplicate.addComponent<Fufu::CameraComponent>(cam);
			}

			if (m_Source.hasComponent<Fufu::GroomComponent>())
				m_Duplicate.addComponent<Fufu::GroomComponent>(m_Source.getComponent<Fufu::GroomComponent>());

			if (m_Source.hasComponent<Fufu::LightComponent>())
				m_Duplicate.addComponent<Fufu::LightComponent>(m_Source.getComponent<Fufu::LightComponent>());

			if (m_Source.hasComponent<Fufu::PrefabInstanceComponent>())
				m_Duplicate.addComponent<Fufu::PrefabInstanceComponent>(m_Source.getComponent<Fufu::PrefabInstanceComponent>());

			if (m_Source.hasComponent<Fufu::ParentComponent>())
			{
				entt::entity parentHandle = m_Source.getComponent<Fufu::ParentComponent>().parent;
				Fufu::Entity parent(parentHandle, m_Scene.get());
				if (parent.isValid())
					m_Scene->setParent(m_Duplicate, parent);
			}
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

	// Destroys an entity and keeps a full snapshot (components + hierarchy)
	// to be able to restore it identically on undo.
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

			// Re-attach children that were pointing to the destroyed entity
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

	// Changes an entity's parent (drag & drop in the Hierarchy). An
	// invalid newParent means "unparent" (used by the Unparent action).
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

		void execute() override
		{
			if (m_NewParent && m_NewParent.isValid())
				m_Scene->setParent(m_Child, m_NewParent);
			else
				m_Scene->removeParent(m_Child);
		}

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

	// Groups multiple entities under a new empty entity (transforms are in world
	// space in this engine: no recalculation is needed, grouped entities
	// visually retain their position).
	class EntityGroupCommand : public ICommand
	{
	public:
		EntityGroupCommand(std::shared_ptr<Fufu::Scene> scene, std::vector<Fufu::Entity> targets,
			std::string tag = "Group")
			: m_Scene(std::move(scene)), m_Targets(std::move(targets)), m_Tag(std::move(tag))
		{
			m_OldParents.reserve(m_Targets.size());
			for (Fufu::Entity target : m_Targets)
			{
				Fufu::Entity oldParent;
				if (target.hasComponent<Fufu::ParentComponent>())
					oldParent = Fufu::Entity(target.getComponent<Fufu::ParentComponent>().parent, m_Scene.get());

				m_OldParents.push_back(oldParent);
			}
		}

		void execute() override
		{
			m_Group = m_Scene->createEntity(m_Tag);

			for (Fufu::Entity target : m_Targets)
			{
				if (target.isValid())
					m_Scene->setParent(target, m_Group);
			}
		}

		void undo() override
		{
			for (std::size_t i = 0; i < m_Targets.size(); ++i)
			{
				if (!m_Targets[i].isValid())
					continue;

				if (m_OldParents[i].isValid())
					m_Scene->setParent(m_Targets[i], m_OldParents[i]);
				else
					m_Scene->removeParent(m_Targets[i]);
			}

			if (m_Group.isValid())
				m_Scene->destroyEntity(m_Group);

			m_Group = {};
		}

		const char* getName() const override { return "Group Entities"; }

		Fufu::Entity getEntity() const { return m_Group; }

	private:
		std::shared_ptr<Fufu::Scene> m_Scene;
		std::vector<Fufu::Entity> m_Targets;
		std::vector<Fufu::Entity> m_OldParents;
		std::string m_Tag;
		Fufu::Entity m_Group;
	};
}
