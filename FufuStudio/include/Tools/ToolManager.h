#pragma once

#include "IEditorTool.h"
#include <memory>
#include <vector>

namespace FufuStudio
{
	// Registre des outils du viewport + outil actuellement actif.
	class ToolManager
	{
	public:
		IEditorTool* registerTool(std::unique_ptr<IEditorTool> tool)
		{
			m_Tools.push_back(std::move(tool));
			return m_Tools.back().get();
		}

		void setActiveTool(std::size_t index)
		{
			if (index < m_Tools.size())
				m_ActiveIndex = index;
		}

		IEditorTool* getActiveTool() const
		{
			return m_Tools.empty() ? nullptr : m_Tools[m_ActiveIndex].get();
		}

		const std::vector<std::unique_ptr<IEditorTool>>& getTools() const { return m_Tools; }

	private:
		std::vector<std::unique_ptr<IEditorTool>> m_Tools;
		std::size_t m_ActiveIndex = 0;
	};
}
