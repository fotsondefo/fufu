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
		char        m_RenameBuffer[128] = {};
		std::string m_RenamingSceneName; // quelle scène est en cours de renommage
		bool        m_RenamingScene = false;
	};

}