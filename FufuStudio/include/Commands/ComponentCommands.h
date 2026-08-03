#pragma once

#include "ICommand.h"
#include <Project/Entity.h>
#include <Project/Components.h>
#include <cstdint>
#include <string>
#include <utility>

namespace FufuStudio
{
	// Generic undo/redo for in-place editing of an existing component
	// (Transform, Material, Camera, Tag...). Capture le component entier
	// before/after rather than an isolated field: simpler, and the undo granularity
	// corresponds to "one completed interaction" (releasing the slider/gizmo).
	template<typename Component>
	class ComponentEditCommand : public ICommand
	{
	public:
		ComponentEditCommand(Fufu::Entity entity, Component before, Component after)
			: m_Entity(entity), m_Before(std::move(before)), m_After(std::move(after)) {}

		void execute() override { apply(m_After); }
		void undo() override { apply(m_Before); }
		const char* getName() const override { return "Edit Component"; }

	private:
		void apply(const Component& value)
		{
			if (m_Entity.isValid() && m_Entity.hasComponent<Component>())
			{
				m_Entity.getComponent<Component>() = value;
				// In-place edit (no add/removeComponent): the only case that
				// Entity's structural hooks do not cover automatically.
				if (auto* scene = m_Entity.getScene())
					scene->markDirty();
			}
		}

		Fufu::Entity m_Entity;
		Component m_Before;
		Component m_After;
	};

	// Adds a default-constructed component to an entity. undo() removes it.
	template<typename Component>
	class ComponentAddCommand : public ICommand
	{
	public:
		explicit ComponentAddCommand(Fufu::Entity entity) : m_Entity(entity) {}

		void execute() override
		{
			if (m_Entity.isValid() && !m_Entity.hasComponent<Component>())
				m_Entity.addComponent<Component>();
		}

		void undo() override
		{
			if (m_Entity.isValid() && m_Entity.hasComponent<Component>())
				m_Entity.removeComponent<Component>();
		}

		const char* getName() const override { return "Add Component"; }

	private:
		Fufu::Entity m_Entity;
	};

	// Removes a component, keeping its value so it can be restored on undo.
	template<typename Component>
	class ComponentRemoveCommand : public ICommand
	{
	public:
		explicit ComponentRemoveCommand(Fufu::Entity entity) : m_Entity(entity) {}

		void execute() override
		{
			if (m_Entity.isValid() && m_Entity.hasComponent<Component>())
			{
				m_Backup = m_Entity.getComponent<Component>();
				m_Entity.removeComponent<Component>();
			}
		}

		void undo() override
		{
			if (m_Entity.isValid() && !m_Entity.hasComponent<Component>())
				m_Entity.addComponent<Component>(m_Backup);
		}

		const char* getName() const override { return "Remove Component"; }

	private:
		Fufu::Entity m_Entity;
		Component m_Backup{};
	};

	// Sets (adds or replaces) the MeshComponent of an entity — used by
	// dragging a mesh asset (Viewport, Hierarchy, Inspector). Unlike
	// ComponentEditCommand/ComponentAddCommand, works whether the entity
	// already has a mesh or not, and takes arguments (MeshComponent is not
	// just default-constructed).
	class SetMeshCommand : public ICommand
	{
	public:
		SetMeshCommand(Fufu::Entity entity, std::string meshPath, uint64_t meshID)
			: m_Entity(entity), m_NewPath(std::move(meshPath)), m_NewID(meshID)
		{
			m_HadBefore = entity.isValid() && entity.hasComponent<Fufu::MeshComponent>();
			if (m_HadBefore)
				m_Before = entity.getComponent<Fufu::MeshComponent>();
		}

		void execute() override
		{
			if (!m_Entity.isValid()) return;

			if (m_Entity.hasComponent<Fufu::MeshComponent>())
			{
				m_Entity.getComponent<Fufu::MeshComponent>() = Fufu::MeshComponent(m_NewPath, m_NewID);
				if (auto* scene = m_Entity.getScene()) scene->markDirty();
			}
			else
			{
				m_Entity.addComponent<Fufu::MeshComponent>(m_NewPath, m_NewID);
			}
		}

		void undo() override
		{
			if (!m_Entity.isValid()) return;

			if (m_HadBefore)
			{
				m_Entity.getComponent<Fufu::MeshComponent>() = m_Before;
				if (auto* scene = m_Entity.getScene()) scene->markDirty();
			}
			else if (m_Entity.hasComponent<Fufu::MeshComponent>())
			{
				m_Entity.removeComponent<Fufu::MeshComponent>();
			}
		}

		const char* getName() const override { return "Set Mesh"; }

	private:
		Fufu::Entity m_Entity;
		std::string m_NewPath;
		uint64_t m_NewID;
		bool m_HadBefore = false;
		Fufu::MeshComponent m_Before;
	};
}
