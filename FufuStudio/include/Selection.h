#pragma once

#include <Project/Entity.h>
#include <algorithm>
#include <vector>

namespace FufuStudio
{
	// Sélection multi-entités de l'éditeur. Le dernier élément ajouté est la
	// "primaire" : c'est elle que l'Inspector édite et sur laquelle le gizmo
	// se positionne (pivot) quand plusieurs entités sont sélectionnées.
	class Selection
	{
	public:
		// Remplace la sélection par une seule entité (clic simple).
		void select(Fufu::Entity entity)
		{
			m_Entities.clear();
			if (entity && entity.isValid())
				m_Entities.push_back(entity);
		}

		// Ajoute/retire une entité de la sélection (Ctrl+clic).
		void toggle(Fufu::Entity entity)
		{
			if (!entity || !entity.isValid())
				return;

			auto it = std::find(m_Entities.begin(), m_Entities.end(), entity);
			if (it != m_Entities.end())
				m_Entities.erase(it);
			else
				m_Entities.push_back(entity);
		}

		void clear() { m_Entities.clear(); }

		bool isSelected(Fufu::Entity entity) const
		{
			return std::find(m_Entities.begin(), m_Entities.end(), entity) != m_Entities.end();
		}

		bool empty() const { return m_Entities.empty(); }
		std::size_t size() const { return m_Entities.size(); }

		// Entité principale (dernière ajoutée) : celle que l'Inspector affiche
		// et sur laquelle le gizmo se positionne.
		Fufu::Entity primary() const
		{
			return m_Entities.empty() ? Fufu::Entity{} : m_Entities.back();
		}

		const std::vector<Fufu::Entity>& entities() const { return m_Entities; }

	private:
		std::vector<Fufu::Entity> m_Entities;
	};
}
