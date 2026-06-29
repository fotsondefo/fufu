#pragma once

#include "EditorState.h"

namespace FufuStudio 
{

	class WelcomeScreen
	{
	public:
		bool onImGuiRender(EditorState& state);

	private:
		void drawRecentProjects(EditorState& state);
		void drawNewProject(EditorState& state);

		char m_NewProjectName[128] = "MyProject";
		char m_NewProjectDir[512] = "";
		bool m_ShowNewProject = false;
	};

}