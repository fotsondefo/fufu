#pragma once

#include <Project/Entity.h>
#include <algorithm>
#include <vector>

namespace FufuStudio
{
	// Multi-entity editor selection. The last added element is the
	// "primary": it is the one the Inspector edits and on which the gizmo
	// positions itself (pivot) when multiple entities are selected.
	class Selection
	{
	public:
		// Replaces the selection with a single entity (simple click).
		void select(Fufu::Entity entity)
		{
			m_Entities.clear();
			if (entity && entity.isValid())
				m_Entities.push_back(entity);
		}

		// Adds/removes an entity from the selection (Ctrl+click).
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

		// Primary entity (last added): the one the Inspector displays
		// and on which the gizmo positions itself.
		Fufu::Entity primary() const
		{
			return m_Entities.empty() ? Fufu::Entity{} : m_Entities.back();
		}

		const std::vector<Fufu::Entity>& entities() const { return m_Entities; }

	private:
		std::vector<Fufu::Entity> m_Entities;
	};
}
