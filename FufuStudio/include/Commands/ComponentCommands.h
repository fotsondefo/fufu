#pragma once

#include "ICommand.h"
#include <Project/Entity.h>
#include <Project/Components.h>
#include <cstdint>
#include <string>
#include <utility>

namespace FufuStudio
{
	// Undo/redo g�n�rique pour l'�dition en place d'un component existant
	// (Transform, Material, Camera, Tag...). Capture le component entier
	// avant/apr�s plut�t qu'un champ isol� : plus simple, et le grain d'undo
	// correspond � "une interaction termin�e" (relacher le slider/le gizmo).
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
				// Édition en place (pas d'add/removeComponent) : le seul cas que
				// les hooks structurels d'Entity ne couvrent pas automatiquement.
				if (auto* scene = m_Entity.getScene())
					scene->markDirty();
			}
		}

		Fufu::Entity m_Entity;
		Component m_Before;
		Component m_After;
	};

	// Ajoute un component par d�faut sur une entit�. undo() le retire.
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

	// Retire un component, en gardant sa valeur pour pouvoir le restaurer au undo.
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

	// Définit (ajoute ou remplace) le MeshComponent d'une entité — utilisé par
	// le drop d'un asset mesh (Viewport, Hierarchy, Inspector). Contrairement
	// à ComponentEditCommand/ComponentAddCommand, fonctionne que l'entité ait
	// déjà un mesh ou non, et prend des arguments (MeshComponent n'est pas
	// juste défaut-construit).
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
