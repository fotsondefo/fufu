#pragma once

#include "Panels/IEditorPanel.h"

namespace FufuStudio 
{

	class HierarchyPanel : public IEditorPanel
	{
	public:
		void onImGuiRender(EditorState& state) override;

	private:
		void drawEntityNode(Fufu::Entity entity, EditorState& state);
		void drawContextMenu(EditorState& state);
	};

}