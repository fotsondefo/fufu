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
		void drawAssetBrowser(EditorState& state);
		void drawDirTree(const std::filesystem::path& dir);

		std::filesystem::path m_SelectedAsset;
		std::filesystem::path m_BrowserDir;   // currently displayed directory
		char m_RenameBuffer[128]  = {};
		char m_SearchBuf[128]     = {};
		std::string m_RenamingSceneName;
		bool        m_RenamingScene = false;
	};

}