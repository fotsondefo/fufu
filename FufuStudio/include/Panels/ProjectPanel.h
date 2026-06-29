#pragma once

#include "Panels/IEditorPanel.h"

namespace FufuStudio 
{

	class ProjectPanel : public IEditorPanel
	{
	public:
		void onImGuiRender(EditorState& state) override;

	private:
		void drawProjectHeader();
		void drawSceneList(EditorState& state);
		void drawAssetTree(EditorState& state);

		std::filesystem::path m_SelectedAsset;
		std::string m_RenameBuffer;
		bool m_RenamingScene = false;
	};

}