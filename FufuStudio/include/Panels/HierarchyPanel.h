#pragma once

#include "Panels/IEditorPanel.h"
#include <memory>
#include <vector>

namespace Fufu { class Scene; }

namespace FufuStudio
{

	class HierarchyPanel : public IEditorPanel
	{
	public:
		void onImGuiRender(EditorState& state) override;

	private:
		void drawEntityNode(Fufu::Entity entity, EditorState& state);
		void drawContextMenu(EditorState& state);

		// Deletes one or more entities as a single history command.
		void deleteEntities(EditorState& state, std::shared_ptr<Fufu::Scene> scene,
			const std::vector<Fufu::Entity>& targets);
	};

}